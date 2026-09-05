#include "Zigbee.h"
#include <Preferences.h>
#include "esp_system.h"     // esp_reset_reason()
#include "esp_ota_ops.h"    // OTA rollback: mark-valid / running-partition state

/* OTA ANTI-BRICK ROLLBACK — verifyRollbackLater() is defined below (after the
 * data types), returning true so the arduino-esp32 core does NOT auto-confirm a
 * freshly-OTA'd image; loop() confirms it only after a health check. */

/*
 * ESP32-C6 ZIGBEE IN-WALL SCENE / BUTTON MODULE  (v4)
 * --------------------------------------------------
 * Mains powered (HLK-PM01 -> AMS1117-3.3 -> WT0132C6-S5), joins as a ZIGBEE
 * ROUTER. Two wall inputs reported to Z2M as button ACTIONS.
 *
 * v6 (0x01000005):
 *   - OTA anti-brick ROLLBACK: a freshly-OTA'd image is only confirmed after it
 *     stays joined OTA_VALIDATE_MS; a crash-looping bad OTA auto-reverts to the
 *     previous good image (see verifyRollbackLater above). v5 (0x01000004, no
 *     rollback) remains the wired-flashed trusted fallback baseline.
 *   - Per-channel INPUT MODE, configurable from the Z2M profile (written to a
 *     Multistate Output, persisted in NVS): momentary / toggle / toggle_directional
 *     / toggle_scenes. Lets the same unit work with a momentary push button OR a
 *     bi-stable (rocker) toggle switch. Default = momentary (unchanged behaviour).
 *   - brownout/reboot counter (esp_reset_reason + NVS) exposed as an analog, so a
 *     unit browning out in a wall reveals itself in Z2M with no serial cable.
 *
 * DO NOT re-add temperatureRead()/ZigbeeTempSensor: on the ESP32-C6 the 802.15.4
 * radio owns the internal temperature-sensor peripheral, so calling temperatureRead()
 * once the radio is up collides with it and hard-faults -> reboot loop (this bit v4).
 *
 * Transport:
 *   EP10/EP11 = Multistate Input (actions) + Multistate Output (mode) [+OTA on EP10]
 *   EP13      = Analog Input (genAnalogInput)          -> brownout_count
 *   Action presentValue codes: 1 single, 2 double, 3 long, 4 toggle, 5 on, 6 off.
 *   Mode  presentValue codes (Multistate Output): 0 momentary, 1 toggle,
 *                                                  2 toggle_directional, 3 toggle_scenes.
 *   See external converter diy_esp32c6_2ch_input.js.
 *
 * Arduino IDE: Board = ESP32C6 Dev Module, Zigbee Mode = ZCZR, Partition = Zigbee ZCZR 4MB.
 */

/* ============================================================
 *  BOARD SELECTION — uncomment exactly ONE
 * ============================================================ */
//#define BOARD_XIAO_ESP32C6    // Breadboard prototype (Seeed XIAO ESP32C6)
#define BOARD_WT0132C6_S5       // Final mains-powered board (WT0132C6-S5, ESP-12F footprint)

#if defined(BOARD_XIAO_ESP32C6) && defined(BOARD_WT0132C6_S5)
#error "Select only ONE board"
#elif !defined(BOARD_XIAO_ESP32C6) && !defined(BOARD_WT0132C6_S5)
#error "Select a board: BOARD_XIAO_ESP32C6 or BOARD_WT0132C6_S5"
#endif

#ifdef BOARD_XIAO_ESP32C6
/* --- XIAO ESP32C6 (prototype) --- */
#define SW1_PIN 1          // D1: wall switch to GND (internal pull-up is enough on a bench)
#define SW2_PIN 2          // D2: wall switch to GND
#define BUTTON_PIN 9       // BOOT button is GPIO 9 (pairing / factory reset)
#define STATUS_LED 15      // XIAO yellow user LED, active LOW
#define HAS_STATUS_LED     // XIAO has a usable user LED
#define HAS_RF_SWITCH      // XIAO has the internal/external antenna RF switch
#endif

#ifdef BOARD_WT0132C6_S5
/* --- WT0132C6-S5 (final board, ESP-12F land pattern) --- */
#define SW1_PIN 4          // IO4: wall switch to GND (ext. 10k pull-up + RC)
#define SW2_PIN 5          // IO5: wall switch to GND (ext. 10k pull-up + RC)
#define BUTTON_PIN 9       // IO9, bottom castellation (pairing / factory reset)
#define STATUS_LED 7       // IO7 (module PIN 10): status LED, ACTIVE LOW. LED trace
                           // cut+rewired from RXD0 to pin 10 (IO7) on both boards, so
                           // UART0 serial stays free. (pin 10 == IO7, NOT IO10.)
#define HAS_STATUS_LED     // active-LOW LED: blink while searching, solid ~3s on join
#endif

/* --- GESTURE TIMING (per input) --- */
#define DEBOUNCE_MS      25    // Raw edge must be stable this long
#define MULTI_WINDOW_MS  300   // Max gap between clicks to count as a multi-click
#define LONG_PRESS_MS    600   // Hold at least this long = 'long'
#define ACTION_CLEAR_MS  150   // After firing, reset multistate 0 so repeats re-report
#define OTA_VALIDATE_MS  90000UL  // Stay joined this long before confirming a new OTA image
                                  // (a crash-loop never reaches it -> bootloader rolls back)

/* --- Action codes reported on genMultistateInput presentValue --- */
#define ACT_IDLE   0
#define ACT_SINGLE 1
#define ACT_DOUBLE 2
#define ACT_LONG   3
#define ACT_TOGGLE 4
#define ACT_ON     5
#define ACT_OFF    6

/* --- Per-channel input MODE (genMultistateOutput presentValue) --- */
#define MODE_MOMENTARY  0   // push button: single / double / long
#define MODE_TOGGLE     1   // rocker: emit 'toggle' on every flip
#define MODE_TOGGLE_DIR 2   // rocker: 'on' when closed, 'off' when open
#define MODE_SCENES     3   // rocker: single / double from flip-count (no long)
#define MODE_COUNT      4

/* --- OTA (Zigbee firmware update over the mesh) --- */
// Version is 0xMMmmpprr — this build is v1.0.0.5 (v6: v5 + OTA rollback guard).
#define OTA_FW_RUNNING     0x01000005
#define OTA_FW_DOWNLOADED  0x01000006
#define OTA_HW_VERSION     0x0101
#define OTA_MANUFACTURER   0x1001
#define OTA_IMAGE_TYPE     0x1011

/* --- ZIGBEE ENDPOINTS --- */
ZigbeeMultistate zbBtn1 = ZigbeeMultistate(10);  // Input=actions ch1, Output=mode ch1, +OTA
ZigbeeMultistate zbBtn2 = ZigbeeMultistate(11);  // Input=actions ch2, Output=mode ch2
ZigbeeAnalog     zbDiag = ZigbeeAnalog(13);       // brownout/reboot counter

/* --- PER-INPUT STATE --- */
struct Button {
  uint8_t pin;
  ZigbeeMultistate *ep;
  uint8_t mode;                 // MODE_* (loaded from NVS, set from Z2M)
  bool rawLast;                 // last raw level read
  bool pressed;                 // debounced level (true = shorted to GND / closed)
  unsigned long lastEdge;       // millis() of last raw transition
  unsigned long pressStart;     // millis() when the current press began
  bool longFired;               // long already emitted for this hold
  uint8_t clickCount;           // clicks / taps accumulated in the current burst
  unsigned long lastRelease;    // millis() of last release / tap
  bool awaiting;                // waiting to see if another click/tap arrives
  unsigned long clearAt;        // millis() to reset presentValue to 0 (0 = idle)
};

Button buttons[2] = {
  { SW1_PIN, &zbBtn1, MODE_MOMENTARY, HIGH, false, 0, 0, false, 0, 0, false, 0 },
  { SW2_PIN, &zbBtn2, MODE_MOMENTARY, HIGH, false, 0, 0, false, 0, 0, false, 0 },
};

Preferences prefs;

// Diagnostics captured at boot
uint32_t g_brownoutCount = 0;
bool diagReported = false;

// Factory-reset button (IO9 / BOOT) tracking
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

#ifdef HAS_RF_SWITCH
bool isExternalAntenna = false; // Tracks current antenna mode
int clickCount = 0;             // Triple-click toggles internal/external antenna
unsigned long lastClickTime = 0;
#endif

/* --- Persist a channel's mode (called from the Z2M-write callback) --- */
void saveMode(uint8_t ch, uint8_t mode) {
  prefs.begin("wallcfg", false);
  prefs.putUChar(ch == 0 ? "mode0" : "mode1", mode);
  prefs.end();
}
// genMultistateOutput write from Z2M -> update channel mode + persist.
void onMode1Change(uint16_t s) { if (s < MODE_COUNT) { buttons[0].mode = s; saveMode(0, s);
  Serial.printf("CH1 mode -> %u\n", s); } }
void onMode2Change(uint16_t s) { if (s < MODE_COUNT) { buttons[1].mode = s; saveMode(1, s);
  Serial.printf("CH2 mode -> %u\n", s); } }

#ifdef HAS_RF_SWITCH
/* --- ANTENNA SWITCH FUNCTION (XIAO only) --- */
void applyAntennaConfig(bool useExternal) {
  pinMode(3, OUTPUT);
  pinMode(14, OUTPUT);
  if (useExternal) {
    digitalWrite(3, LOW);   // Turn ON RF switch power
    digitalWrite(14, HIGH); // Route to U.FL connector
    Serial.println("RF Mode: EXTERNAL (U.FL)");
  } else {
    digitalWrite(3, HIGH);  // Turn OFF RF switch power
    digitalWrite(14, LOW);  // Route to Internal Ceramic
    Serial.println("RF Mode: INTERNAL (Ceramic)");
  }
}
#endif

/* --- RESET HELPER FUNCTION --- */
void triggerFactoryReset() {
  Serial.println("FACTORY RESET TRIGGERED!");
#ifdef HAS_STATUS_LED
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED, LOW);  delay(150);
    digitalWrite(STATUS_LED, HIGH); delay(150);
  }
#else
  delay(500);
#endif
  Zigbee.factoryReset();
}

/* --- EMIT ONE ACTION AS A MULTISTATE REPORT --- */
void fireAction(Button &b, uint8_t code) {
  const char *name;
  switch (code) {
    case ACT_SINGLE: name = "single"; break;
    case ACT_DOUBLE: name = "double"; break;
    case ACT_LONG:   name = "long";   break;
    case ACT_TOGGLE: name = "toggle"; break;
    case ACT_ON:     name = "on";     break;
    case ACT_OFF:    name = "off";    break;
    default:         name = "?";      break;
  }
  Serial.printf("Button GPIO%d -> %s\n", b.pin, name);
  if (Zigbee.connected()) {
    b.ep->setMultistateInput(code);
    b.ep->reportMultistateInput();
    b.clearAt = millis() + ACTION_CLEAR_MS;  // reset to 0 shortly after
  }
}

/* OTA rollback hook: return true so the core leaves a freshly-OTA'd image
 * PENDING_VERIFY; loop() confirms it after OTA_VALIDATE_MS of healthy uptime. */
extern "C" bool verifyRollbackLater() {
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef HAS_STATUS_LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW); // LED ON = Booting
#endif

  // 1. Switch inputs
  for (auto &b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
    b.rawLast = digitalRead(b.pin);
    b.pressed = (b.rawLast == LOW);
  }

  // 2. Load config (modes) + capture reset diagnostics from NVS
  prefs.begin("wallcfg", false);
  buttons[0].mode = prefs.getUChar("mode0", MODE_MOMENTARY);
  buttons[1].mode = prefs.getUChar("mode1", MODE_MOMENTARY);
  g_brownoutCount = prefs.getUInt("bodcnt", 0);
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_BROWNOUT) { g_brownoutCount++; prefs.putUInt("bodcnt", g_brownoutCount); }
  prefs.end();
  Serial.printf("Boot: reset_reason=%d, brownout_count=%u, modes=%u/%u\n",
                (int)rr, g_brownoutCount, buttons[0].mode, buttons[1].mode);

#ifdef HAS_RF_SWITCH
  prefs.begin("zigbee-cfg", false);
  isExternalAntenna = prefs.getBool("ext_ant", false);
  prefs.end();
  applyAntennaConfig(isExternalAntenna);
#endif

  // 3. Zigbee identity
  zbBtn1.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");
  zbBtn2.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");

  // 4. Each channel: Multistate Input (actions, 7 states 0..6) + Multistate Output (mode)
  zbBtn1.addMultistateInput();  zbBtn1.setMultistateInputStates(7);  zbBtn1.setMultistateInputDescription("Button 1");
  zbBtn1.addMultistateOutput(); zbBtn1.setMultistateOutputStates(MODE_COUNT);
  zbBtn2.addMultistateInput();  zbBtn2.setMultistateInputStates(7);  zbBtn2.setMultistateInputDescription("Button 2");
  zbBtn2.addMultistateOutput(); zbBtn2.setMultistateOutputStates(MODE_COUNT);
  zbBtn1.onMultistateOutputChange(onMode1Change);
  zbBtn2.onMultistateOutputChange(onMode2Change);

  // 5. Diagnostics endpoint (brownout counter only — NO temperature; see header note)
  zbDiag.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");
  zbDiag.addAnalogInput();
  zbDiag.setAnalogInputDescription("Brownout count");

  // OTA client on EP10
  zbBtn1.addOTAClient(OTA_FW_RUNNING, OTA_FW_DOWNLOADED, OTA_HW_VERSION,
                      OTA_MANUFACTURER, OTA_IMAGE_TYPE);

  Zigbee.addEndpoint(&zbBtn1);
  Zigbee.addEndpoint(&zbBtn2);
  Zigbee.addEndpoint(&zbDiag);

  Zigbee.begin(ZIGBEE_ROUTER);

  // Reflect the persisted modes back to Z2M so the profile shows the current value.
  zbBtn1.setMultistateOutput(buttons[0].mode);
  zbBtn2.setMultistateOutput(buttons[1].mode);
}

/* --- Run the input state-machine for one channel --- */
void serviceButton(Button &b, unsigned long now) {
  bool raw = digitalRead(b.pin);
  if (raw != b.rawLast) { b.lastEdge = now; b.rawLast = raw; }

  // Detect a debounced edge (level change that has been stable DEBOUNCE_MS)
  bool edge = false, nowPressed = b.pressed;
  if ((now - b.lastEdge) > DEBOUNCE_MS) {
    nowPressed = (raw == LOW);
    if (nowPressed != b.pressed) edge = true;
  }

  switch (b.mode) {
    case MODE_TOGGLE:
      if (edge) { b.pressed = nowPressed; fireAction(b, ACT_TOGGLE); }
      break;

    case MODE_TOGGLE_DIR:
      if (edge) { b.pressed = nowPressed; fireAction(b, nowPressed ? ACT_ON : ACT_OFF); }
      break;

    case MODE_SCENES:
      // Every flip (either direction) counts as a tap -> single / double.
      if (edge) { b.pressed = nowPressed; b.clickCount++; b.lastRelease = now; b.awaiting = true; }
      if (b.awaiting && (now - b.lastRelease) > MULTI_WINDOW_MS) {
        fireAction(b, b.clickCount >= 2 ? ACT_DOUBLE : ACT_SINGLE);
        b.clickCount = 0; b.awaiting = false;
      }
      break;

    case MODE_MOMENTARY:
    default:
      if (edge) {
        b.pressed = nowPressed;
        if (nowPressed) {                 // press
          b.pressStart = now; b.longFired = false;
        } else if (!b.longFired) {         // release
          b.clickCount++;
          if (b.clickCount >= 2) { fireAction(b, ACT_DOUBLE); b.clickCount = 0; b.awaiting = false; }
          else { b.lastRelease = now; b.awaiting = true; }
        }
      }
      if (b.pressed && !b.longFired && (now - b.pressStart) >= LONG_PRESS_MS) {
        fireAction(b, ACT_LONG); b.longFired = true; b.clickCount = 0; b.awaiting = false;
      }
      if (b.awaiting && (now - b.lastRelease) > MULTI_WINDOW_MS) {
        fireAction(b, ACT_SINGLE); b.clickCount = 0; b.awaiting = false;
      }
      break;
  }

  // Reset presentValue to idle so a repeat of the same action re-reports.
  if (b.clearAt && now >= b.clearAt) {
    if (Zigbee.connected()) { b.ep->setMultistateInput(ACT_IDLE); b.ep->reportMultistateInput(); }
    b.clearAt = 0;
  }
}

void loop() {
  unsigned long now = millis();

  // --- 0. Post-connect one-shots: OTA polling + publish boot diagnostics ---
  static bool otaRequested = false;
  if (!otaRequested && Zigbee.connected()) {
    otaRequested = true;
    zbBtn1.requestOTAUpdate();
    Serial.println("OTA update polling started");
  }
  if (!diagReported && Zigbee.connected() && (now > 4000)) {
    diagReported = true;
    // Set the attribute value only. Do NOT call reportAnalogInput(): a proactive
    // genAnalogInput report trips a Zigbee stack assertion (esp_zigbee_zcl_command.c:263)
    // -> panic/reboot loop. Z2M reads brownout_count on interview instead.
    zbDiag.setAnalogInput((float)g_brownoutCount);
  }

  // --- 0b. OTA ROLLBACK: confirm this image only after it has proven healthy
  //     (joined + stayed up OTA_VALIDATE_MS). A crash-loop never reaches here, so
  //     the bootloader auto-reverts to the previous good image. Note: an UNPLANNED
  //     reboot (incl. a power blip) during this window also reverts — keep it short.
  static bool imgValidated = false;
  if (!imgValidated && Zigbee.connected() && now > OTA_VALIDATE_MS) {
    imgValidated = true;
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
      Serial.println("OTA image confirmed healthy (rollback cancelled)");
    }
  }

  // --- 1. INPUTS ---
  for (auto &b : buttons) serviceButton(b, now);

  // --- 2. FACTORY-RESET BUTTON (IO9 / BOOT) ---
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
#ifdef HAS_RF_SWITCH
    if (now - lastClickTime > 600) clickCount = 1; else clickCount++;
    lastClickTime = now;
#endif
    buttonPressed = true;
    buttonPressTime = now;
  }
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    buttonPressed = false;
#ifdef HAS_RF_SWITCH
    if (clickCount == 3) {
      isExternalAntenna = !isExternalAntenna;
      prefs.begin("zigbee-cfg", false); prefs.putBool("ext_ant", isExternalAntenna); prefs.end();
      Serial.println(isExternalAntenna ? "SWITCHED TO EXTERNAL ANTENNA" : "SWITCHED TO INTERNAL ANTENNA");
      for (int i = 0; i < (isExternalAntenna ? 4 : 2); i++) {
        digitalWrite(STATUS_LED, LOW);  delay(250);
        digitalWrite(STATUS_LED, HIGH); delay(250);
      }
      ESP.restart();
    }
#endif
  }
  if (buttonPressed && (now - buttonPressTime > 5000)) {
    triggerFactoryReset();
    buttonPressed = false;
  }
  lastButtonState = currentButtonState;

  // --- 4. CONNECTION STATUS LED ---
#ifdef HAS_STATUS_LED
  static unsigned long lastBlinkTime = 0;
  static bool blinkState = false;
  static bool wasConnected = false;
  static unsigned long connectedTime = 0;
  if (!Zigbee.connected()) {
    if (now - lastBlinkTime > 500) {
      lastBlinkTime = now; blinkState = !blinkState;
      digitalWrite(STATUS_LED, blinkState ? LOW : HIGH);   // blink while searching
    }
    wasConnected = false;
  } else {
    if (!wasConnected) {
      wasConnected = true; connectedTime = now;
      digitalWrite(STATUS_LED, LOW);                        // solid on when connected
    } else if (now - connectedTime > 3000) {
      digitalWrite(STATUS_LED, HIGH);                       // off after 3s (lives in a wall)
    }
  }
#endif

  delay(5);
}
