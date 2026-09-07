#include "Zigbee.h"
#include <Wire.h>
#include <Preferences.h>
#include "esp_system.h"    // esp_reset_reason()
#include "esp_ota_ops.h"   // OTA rollback: mark-valid / running-partition state

/* OTA ANTI-BRICK ROLLBACK — verifyRollbackLater() is defined below, returning
 * true so the arduino-esp32 core does NOT auto-confirm a freshly-OTA'd image;
 * loop() confirms it only after OTA_VALIDATE_MS of healthy joined uptime. */

/*
 * ESP32-C6 ZIGBEE MULTIACCESSORY  (v1, 0x01000000)  — T-0036
 * =========================================================
 * Zigbee replacement for the WiFi/HomeKit (HomeSpan) MultiAccessory firmware.
 * Mains/12 V powered WT0132C6-S5 (ESP32-C6 in the ESP-12F footprint) — joins as
 * a ZIGBEE ROUTER, so every board also strengthens the mesh. HomeKit now comes
 * via HA's bridge instead of directly.
 *
 * Board: 4 PWM MOSFET outputs + a transistor-driven IR LED (LG splits) + I2C
 * (AM2320 temperature/humidity).
 *
 * ONE IMAGE FOR THE WHOLE FLEET
 * -----------------------------
 * Zigbee announces endpoints/clusters at interview time, so composition cannot
 * change on the fly — but it CAN be declared at boot from NVS. The output
 * PROFILE is a Multistate Output on EP14 (writable from the Z2M UI, persisted in
 * NVS); changing it reboots the board, after which Z2M must RE-INTERVIEW it.
 * The AM2320 is auto-detected, so "has a sensor" is not a configuration axis at
 * all: EP20 only exists if the sensor answers.
 *
 * v1 SCOPE (phase 1 of T-0036): PROFILE_4XDIM only, T/H auto-detect, OTA client
 * + rollback guard. CCT_2DIM lands in phase 2 (it is the only other profile the
 * live fleet needs), the composed AC endpoint in phase 3. An unimplemented
 * profile falls back to 4XDIM with a log line rather than bricking the board.
 *
 * ENDPOINT MAP
 *   EP10..EP13  Dimmable Light, one per PWM channel (OTA client sits on EP10)
 *   EP14        Multistate Output -> output profile selector
 *   EP15        Analog Input      -> brownout/reset counter
 *   EP16        Temperature       -> C6 die temperature (diagnostic)
 *   EP20        Temperature + Humidity -> AM2320, ONLY IF DETECTED
 *   EP30        reserved for the AC (phase 3)
 *
 * ⚠️ NO ATTRIBUTE REPORTING ANYWHERE — every Zigbee report path faults this
 * ESP32-C6 zboss build (T-0012, the hard way): app-level report*() asserts in
 * esp_zigbee_zcl_command.c:263, and setReporting()/the coordinator's
 * ConfigureReporting null-deref the stack's own send path
 * (zb_zcl_send_report_attr_command, MTVAL=0x3). So this firmware only ever SETS
 * attribute values and never calls set*Reporting()/report*(). The Z2M converter
 * POLLS with reads (the read-response path is the one that works), and must not
 * pass configureReporting:true — which is already the Z2M default for light().
 * Lights do not need reporting: Z2M is optimistic after a command and reads back.
 *
 * Arduino: Board = ESP32C6 Dev Module, Zigbee Mode = ZCZR, Partition = Zigbee ZCZR 4MB.
 */

/* ============================================================
 *  PIN MAP — the standard WT0132C6-S5 MultiAccessory map.
 *  These are the same PCB pads the ESP8266 build used; the module swap is
 *  proven (both live C6 boards were ESP8266 before, no rework), only the GPIO
 *  numbers behind the pads differ: ch 13/14/16/12 -> 6/5/4/2, IR 10 -> 21,
 *  SDA 4/SCL 5 -> 10/3.
 * ============================================================ */
static const uint8_t CH_PIN[4] = { 6, 5, 4, 2 };  // CH1..CH4
#define IR_LED_PIN   21                            // phase 3
#define SDA_PIN      10
#define SCL_PIN      3
#define STATUS_LED   8
#define CONTROL_PIN  9                             // also IO9/BOOT — factory reset hold
#define STATUS_LED_ACTIVE_LOW 1                    // TODO bench-confirm polarity

/* PWM: 2 kHz is well above visible flicker and gentle on an undriven MOSFET
 * gate; 12-bit gives smooth low-end dimming. */
#define LEDC_FREQ_HZ 2000
#define LEDC_BITS    12
#define DUTY_MAX     ((1 << LEDC_BITS) - 1)

#define FACTORY_RESET_HOLD_MS 5000UL
#define OTA_VALIDATE_MS       90000UL   // stay joined this long before confirming an OTA image
#define DIE_TEMP_PERIOD_MS    60000UL
#define AM2320_PERIOD_MS      120000UL  // matches the HomeKit firmware's 2 min cadence
#define AM2320_ADDR           0x5C

/* OTA identity. NOTE image type 0x1012 — distinct from the wall-input module's
 * 0x1011 so the two DIY devices never see each other's images in the Z2M
 * ota_override index. BUMP OTA_FW_RUNNING EVERY RELEASE (Z2M only offers a
 * strictly-higher fileVersion). */
#define OTA_FW_RUNNING     0x01000000
#define OTA_FW_DOWNLOADED  0x01000001
#define OTA_HW_VERSION     0x0101
#define OTA_MANUFACTURER   0x1001
#define OTA_IMAGE_TYPE     0x1012

#define ZB_MANUFACTURER "DIY"
#define ZB_MODEL        "ESP32C6-MULTIACCESSORY"

/* ============================================================
 *  OUTPUT PROFILES
 * ============================================================ */
enum Profile : uint16_t {
  PROFILE_4XDIM    = 0,  // 4 independent dimmers                     [v1]
  PROFILE_CCT_2DIM = 1,  // CCT on CH1+CH2, dimmers on CH3, CH4       [phase 2]
  PROFILE_2XCCT    = 2,  // CCT on CH1+CH2 and CH3+CH4                [deferred]
  PROFILE_RGBW     = 3,  // R=CH3 G=CH2 B=CH1 W=CH4                   [deferred]
  PROFILE_RGB_DIM  = 4,  // RGB on CH1..CH3, dimmer on CH4            [deferred]
  PROFILE_COUNT    = 5
};
static const char *PROFILE_NAME[PROFILE_COUNT] = {
  "4XDIM", "CCT_2DIM", "2XCCT", "RGBW", "RGB_DIM"
};

static Profile   g_profile        = PROFILE_4XDIM;   // effective (what is running)
static uint16_t  g_profileStored  = PROFILE_4XDIM;   // what NVS says
static uint32_t  g_brownoutCount  = 0;
static bool      g_haveAM2320     = false;
static bool      g_imgValidated   = false;

Preferences prefs;

/* ============================================================
 *  ENDPOINTS
 * ============================================================ */
ZigbeeDimmableLight zbCh1(10);
ZigbeeDimmableLight zbCh2(11);
ZigbeeDimmableLight zbCh3(12);
ZigbeeDimmableLight zbCh4(13);
ZigbeeDimmableLight *zbCh[4] = { &zbCh1, &zbCh2, &zbCh3, &zbCh4 };

ZigbeeMultistate  zbCfg(14);    // profile selector
ZigbeeAnalog      zbDiag(15);   // brownout counter
ZigbeeTempSensor  zbDie(16);    // die temperature
ZigbeeTempSensor  zbRoom(20);   // AM2320 (added only when present)

/* ============================================================
 *  PWM OUTPUT
 * ============================================================ */
/* Perceptual curve: Zigbee CurrentLevel is linear, human brightness is not.
 * gamma 2.2 keeps the bottom of the range usable on LED strips (a linear duty
 * makes 1..40 look almost identical). */
static uint32_t levelToDuty(uint8_t level) {
  if (level == 0) return 0;
  float norm = (float)level / 254.0f;
  float duty = powf(norm, 2.2f) * (float)DUTY_MAX;
  uint32_t d = (uint32_t)(duty + 0.5f);
  return d ? d : 1;  // never round a non-zero level down to fully off
}

/* Single seam for every output write — phase 2's transition ramp slots in here
 * (target vs current + a stepper in loop()) without touching the callbacks. */
static void applyChannel(uint8_t ch, bool state, uint8_t level) {
  if (ch >= 4) return;
  ledcWrite(CH_PIN[ch], state ? levelToDuty(level) : 0);
}

static void onCh(uint8_t ch, bool state, uint8_t level) {
  Serial.printf("CH%u -> %s level=%u\n", ch + 1, state ? "ON" : "OFF", level);
  applyChannel(ch, state, level);
}
/* onLightChange takes a bare function pointer (no user context), so one thunk
 * per channel. */
static void onCh1(bool s, uint8_t l) { onCh(0, s, l); }
static void onCh2(bool s, uint8_t l) { onCh(1, s, l); }
static void onCh3(bool s, uint8_t l) { onCh(2, s, l); }
static void onCh4(bool s, uint8_t l) { onCh(3, s, l); }

/* ============================================================
 *  PROFILE SELECTOR (EP14)
 * ============================================================ */
static void onProfileWrite(uint16_t state) {
  if (state >= PROFILE_COUNT) {
    Serial.printf("Profile %u out of range, ignored\n", state);
    return;
  }
  if (state == g_profileStored) return;

  prefs.begin("macfg", false);
  prefs.putUShort("profile", state);
  prefs.end();
  g_profileStored = state;

  /* The endpoint composition is fixed at interview time, so a profile change
   * only takes effect after a reboot — and Z2M must re-interview the device
   * afterwards to pick up the new endpoints. */
  Serial.printf("Profile -> %s, rebooting to apply (re-interview in Z2M after)\n",
                PROFILE_NAME[state]);
  delay(250);
  ESP.restart();
}

/* ============================================================
 *  AM2320 — probe + read, no external library
 * ============================================================ */
static uint16_t crc16Modbus(const uint8_t *buf, uint8_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *buf++;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
      else              { crc >>= 1; }
    }
  }
  return crc;
}

/* The AM2320 sleeps between reads and NAKs the wake-up transaction, so a single
 * probe is not conclusive: wake, pause, then see whether it ACKs. */
static bool am2320Probe() {
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(AM2320_ADDR);
    Wire.endTransmission();          // wake-up (expected to NAK)
    delay(3);
    Wire.beginTransmission(AM2320_ADDR);
    if (Wire.endTransmission() == 0) return true;
    delay(50);
  }
  return false;
}

static bool am2320Read(float &tempC, float &rh) {
  Wire.beginTransmission(AM2320_ADDR);
  Wire.endTransmission();            // wake
  delay(3);

  Wire.beginTransmission(AM2320_ADDR);
  Wire.write(0x03);                  // read registers
  Wire.write(0x00);                  // from 0x00
  Wire.write(0x04);                  // 4 bytes (RH hi/lo, T hi/lo)
  if (Wire.endTransmission() != 0) return false;
  delay(3);

  uint8_t b[8];
  if (Wire.requestFrom(AM2320_ADDR, (uint8_t)8) != 8) return false;
  for (uint8_t i = 0; i < 8; i++) b[i] = Wire.read();
  if (b[0] != 0x03 || b[1] != 0x04) return false;

  uint16_t crcRx = (uint16_t)b[7] << 8 | b[6];   // little-endian on the wire
  if (crcRx != crc16Modbus(b, 6)) return false;

  rh = ((uint16_t)b[2] << 8 | b[3]) / 10.0f;
  uint16_t raw = (uint16_t)(b[4] & 0x7F) << 8 | b[5];
  tempC = raw / 10.0f;
  if (b[4] & 0x80) tempC = -tempC;                // sign lives in the MSB
  return true;
}

/* ============================================================
 *  STATUS LED
 * ============================================================ */
static void ledOn(bool on) {
#if STATUS_LED_ACTIVE_LOW
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
#else
  digitalWrite(STATUS_LED, on ? HIGH : LOW);
#endif
}

/* OTA rollback hook: leave a freshly-OTA'd image PENDING_VERIFY; loop()
 * confirms it after OTA_VALIDATE_MS of healthy joined uptime, so a
 * crash-looping bad image auto-reverts to the previous good one. */
extern "C" bool verifyRollbackLater() {
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  ledOn(true);                       // LED on = booting
  pinMode(CONTROL_PIN, INPUT_PULLUP);

  // 1. PWM outputs first, so nothing floats a MOSFET gate while Zigbee starts.
  for (uint8_t i = 0; i < 4; i++) {
    ledcAttach(CH_PIN[i], LEDC_FREQ_HZ, LEDC_BITS);
    ledcWrite(CH_PIN[i], 0);
  }

  // 2. Config + reset diagnostics from NVS
  prefs.begin("macfg", false);
  g_profileStored = prefs.getUShort("profile", PROFILE_4XDIM);
  g_brownoutCount = prefs.getUInt("bodcnt", 0);
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_BROWNOUT) { g_brownoutCount++; prefs.putUInt("bodcnt", g_brownoutCount); }
  prefs.end();

  g_profile = (Profile)g_profileStored;
  if (g_profile != PROFILE_4XDIM) {
    // Phase 1 only implements 4XDIM. Never brick a board over a config value:
    // run the profile we can and say so.
    Serial.printf("Profile %s not implemented in this build — running 4XDIM\n",
                  g_profileStored < PROFILE_COUNT ? PROFILE_NAME[g_profileStored] : "?");
    g_profile = PROFILE_4XDIM;
  }
  Serial.printf("Boot: reset_reason=%d brownout_count=%u profile=%s(stored %u)\n",
                (int)rr, g_brownoutCount, PROFILE_NAME[g_profile], g_profileStored);

  // 3. I2C + sensor auto-detect. EP20 exists only if the AM2320 answers, so
  //    "has a sensor" never has to be configured.
  Wire.begin(SDA_PIN, SCL_PIN);
  g_haveAM2320 = am2320Probe();
  Serial.printf("AM2320: %s\n", g_haveAM2320 ? "detected -> EP20" : "absent");

  // 4. Light endpoints, one per channel (4XDIM)
  for (uint8_t i = 0; i < 4; i++) {
    zbCh[i]->setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  }
  zbCh1.onLightChange(onCh1);
  zbCh2.onLightChange(onCh2);
  zbCh3.onLightChange(onCh3);
  zbCh4.onLightChange(onCh4);

  // 5. Profile selector
  zbCfg.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  zbCfg.addMultistateOutput();
  zbCfg.setMultistateOutputStates(PROFILE_COUNT);
  zbCfg.setMultistateOutputDescription("Output profile");
  zbCfg.onMultistateOutputChange(onProfileWrite);

  // 6. Diagnostics: brownout counter + die temperature. Values are only SET
  //    here; the converter polls them (see the no-reporting note in the header).
  zbDiag.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  zbDiag.addAnalogInput();
  zbDiag.setAnalogInputDescription("Brownout count");
  zbDie.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  zbDie.setMinMaxValue(-40, 125);

  // 7. Room sensor (only when fitted)
  if (g_haveAM2320) {
    zbRoom.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbRoom.setMinMaxValue(-40, 80);
    zbRoom.setTolerance(0.5);
    zbRoom.addHumiditySensor(0, 100, 1.0, 0);
  }

  // 8. OTA client on EP10 — ships in v1 on purpose: flashing Zigbee removes the
  //    WiFi OTA path, and most of these boards are behind furniture.
  zbCh1.addOTAClient(OTA_FW_RUNNING, OTA_FW_DOWNLOADED, OTA_HW_VERSION,
                     OTA_MANUFACTURER, OTA_IMAGE_TYPE);

  Zigbee.addEndpoint(&zbCh1);
  Zigbee.addEndpoint(&zbCh2);
  Zigbee.addEndpoint(&zbCh3);
  Zigbee.addEndpoint(&zbCh4);
  Zigbee.addEndpoint(&zbCfg);
  Zigbee.addEndpoint(&zbDiag);
  Zigbee.addEndpoint(&zbDie);
  if (g_haveAM2320) Zigbee.addEndpoint(&zbRoom);

  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee.begin() failed — rebooting");
    delay(1000);
    ESP.restart();
  }

  Serial.print("Joining");
  while (!Zigbee.connected()) { Serial.print("."); delay(100); }
  Serial.println(" joined");
  ledOn(false);

  // 9. Publish the current profile and the boot-time diagnostics.
  zbCfg.setMultistateOutput(g_profileStored);
  zbDiag.setAnalogInput((float)g_brownoutCount);

  // 10. Restore each channel from its persisted attributes so a power cut does
  //     not leave strips in a surprise state. This fires the change callbacks,
  //     which drive the PWM. (Proper StartUpOnOff/StartUpCurrentLevel handling
  //     is phase 2.)
  for (uint8_t i = 0; i < 4; i++) zbCh[i]->restoreLight();
}

void loop() {
  const unsigned long now = millis();

  // Factory reset: hold the control pin (IO9) for FACTORY_RESET_HOLD_MS.
  static unsigned long holdStart = 0;
  if (digitalRead(CONTROL_PIN) == LOW) {
    if (holdStart == 0) holdStart = now;
    else if (now - holdStart > FACTORY_RESET_HOLD_MS) {
      Serial.println("Factory reset — leaving the network");
      ledOn(true);
      delay(200);
      Zigbee.factoryReset();
    }
  } else {
    holdStart = 0;
  }

  // Die temperature — SET only, polled by the converter. Never report*().
  static unsigned long lastDie = 0;
  if (now - lastDie >= DIE_TEMP_PERIOD_MS) {
    lastDie = now;
    zbDie.setTemperature(temperatureRead());
  }

  // Room sensor — SET only, same rule.
  static unsigned long lastRoom = 0;
  if (g_haveAM2320 && now - lastRoom >= AM2320_PERIOD_MS) {
    lastRoom = now;
    float t, h;
    if (am2320Read(t, h)) {
      zbRoom.setTemperature(t);
      zbRoom.setHumidity(h);
      Serial.printf("AM2320: %.1f C  %.1f %%\n", t, h);
    } else {
      Serial.println("AM2320: read failed (sensor busy or CRC)");
    }
  }

  // OTA rollback confirmation: only once we have been joined and healthy for
  // OTA_VALIDATE_MS. A crash-looping bad image never reaches this, so the
  // bootloader reverts it.
  if (!g_imgValidated && Zigbee.connected() && now > OTA_VALIDATE_MS) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
      Serial.println("OTA image confirmed (rollback cancelled)");
    }
    g_imgValidated = true;
  }

  delay(50);
}
