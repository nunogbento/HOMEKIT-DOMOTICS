#include "Zigbee.h"
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_system.h"     // esp_reset_reason()
#include "esp_ota_ops.h"    // OTA rollback: mark-valid / running-partition state
#include "esp_core_dump.h"  // post-mortem summary, read back over Zigbee (no serial in the plant room)

/*
 * ESP32-C6 ZIGBEE SOLAR-THERMAL MONITOR  (v1, 0x01000001)
 * -------------------------------------------------------
 * Replaces the ESP8266 + ADS1115 "HKSolarPanel/mqtt-SolarPanel" board (HOMEKIT-DOMOTICS/ESP8266-ADC).
 * Mains powered (HLK-PM01 -> 3V3 -> WT0132C6-S5), joins as a ZIGBEE ROUTER. Monitoring only — it
 * touches neither the Bosch TDS 100-2 solar controller nor the electric backup heater.
 *
 * WHAT IT MEASURES
 *   3x DS18B20 on one OneWire bus:  tank (the 500 L store, as before) + two strap-on pipe probes,
 *       HOT OUT (DHW outlet) and COLD IN (mains inlet). The pipe pair is the hot-water DRAW detector
 *       the T-0035 smart-backup algorithm needs: idle = both at room temp; a draw = hot pipe jumps
 *       toward tank temp within seconds while the cold pipe drops toward mains temp. Duration x a
 *       calibrated flow gives litres; the tank probe alone cannot see a morning shower (2026-09-19).
 *   2x clip-on CTs (100 A / 50 mA, 33 R burden, VDD/2 bias) on an ADS1115, true-RMS:
 *       ch0-1 = ELECTRIC HEATER phase, ch2-3 = SOLAR CIRCULATOR phase (same front-end as the 8266 board).
 *   Collector temperature: NOT here (yet). The TDS 100-2 has no bus and its NTC 20K probe may not be
 *   SELV-referenced — see HARDWARE.md before wiring anything to the controller.
 *
 * TRANSPORT (see diy_esp32c6_solar_monitor.js)
 *   EP10  msTemperatureMeasurement  probe A (lowest ROM)  [+ OTA client]      \
 *   EP11  msTemperatureMeasurement  probe B                                     > roles mapped in Z2M
 *   EP12  msTemperatureMeasurement  probe C (highest ROM)                      /
 *   EP13  msTemperatureMeasurement  C6 die temperature (diagnostic)
 *   EP14  genAnalogInput            heater current, A RMS
 *   EP15  genAnalogInput            circulator current, A RMS
 *   EP16  genAnalogInput/Output     descriptions = crash post-mortem; output write 13579 = crash test
 *
 * NO ZIGBEE REPORTS ARE SENT BY THE APP. Every report path faults this zboss build (see the wall
 * module's history: app report_attr_cmd asserts, stack ConfigureReporting null-derefs). The firmware
 * only refreshes attribute VALUES every MEASURE_MS; the converter POLLS them (read-response works).
 * Z2M must never configureReporting on these endpoints.
 *
 * Arduino IDE: Board = ESP32C6 Dev Module, Zigbee Mode = ZCZR, Partition = Zigbee ZCZR 4MB.
 */

/* ============================================================
 *  BOARD PINS (WT0132C6-S5, ESP-12F land pattern) — PROPOSAL, confirm against the PCB
 * ============================================================ */
#define ONEWIRE_PIN   4    // IO4: DS18B20 bus, 4k7 pull-up to 3V3
#define I2C_SDA       5    // IO5: ADS1115 SDA
#define I2C_SCL       6    // IO6: ADS1115 SCL
#define STATUS_LED    7    // IO7: status LED, ACTIVE LOW (same as the wall module)
#define BUTTON_PIN    9    // IO9 / BOOT: pairing / factory reset (hold 5 s)
#define HAS_STATUS_LED

/* --- TIMING --- */
#define MEASURE_MS        60000UL  // refresh all attributes every 60 s (converter polls at the same cadence)
#define DS_CONVERT_MS       800UL  // 12-bit DS18B20 conversion time (non-blocking wait)
#define CT_SAMPLE_MS        400UL  // RMS window per channel = 20 mains cycles
#define OTA_VALIDATE_MS   90000UL  // stay joined this long before confirming a new OTA image

/* --- CT front-end (identical to ESP8266-ADC) --- */
#define ADS_GAIN        GAIN_ONE    // +-4.096 V, 0.125 mV/bit
#define ADS_MV_PER_BIT  0.125f
#define CT_FACTOR       60.6f       // 100 A / 50 mA CT into 33 R: 100 A -> 1.65 V  => A per V
#define HEATER_ON_A     1.0f        // firmware-side "running" thresholds, mirrored in the converter
#define CIRC_ON_A       0.1f

/* --- OTA (Zigbee firmware update over the mesh) --- */
#define OTA_FW_RUNNING     0x01000001
#define OTA_FW_DOWNLOADED  0x01000002
#define OTA_HW_VERSION     0x0101
#define OTA_MANUFACTURER   0x1001
#define OTA_IMAGE_TYPE     0x1012   // distinct from the wall module (0x1011) — never cross-flash

/* --- ZIGBEE ENDPOINTS --- */
ZigbeeTempSensor zbProbe[3] = { ZigbeeTempSensor(10), ZigbeeTempSensor(11), ZigbeeTempSensor(12) };
ZigbeeTempSensor zbDie      = ZigbeeTempSensor(13);
ZigbeeAnalog     zbHeater   = ZigbeeAnalog(14);
ZigbeeAnalog     zbCirc     = ZigbeeAnalog(15);
ZigbeeAnalog     zbDiag     = ZigbeeAnalog(16);

/* --- SENSORS --- */
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature ds(&oneWire);
DeviceAddress probeAddr[3];
uint8_t probeCount = 0;
Adafruit_ADS1115 ads;
bool adsOk = false;

Preferences prefs;
uint32_t g_brownoutCount = 0;
bool diagReported = false;

// Factory-reset button (IO9 / BOOT)
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

/* OTA rollback hook: leave a freshly-OTA'd image PENDING_VERIFY; loop() confirms it after
 * OTA_VALIDATE_MS of healthy uptime, so a crash-looping image auto-reverts. */
extern "C" bool verifyRollbackLater() { return true; }

/* ============================================================
 *  POST-MORTEM DIAGNOSTICS (identical to the wall module — see its comments)
 * ============================================================ */
#define DIAG_FIELD_MAX 32
#define DIAG_CRASH_MAGIC 13579.0f
static char     g_diagA[DIAG_FIELD_MAX + 1] = "";
static char     g_diagB[DIAG_FIELD_MAX + 1] = "";
static uint32_t g_resetCount = 0;

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

static void buildDiagSummary(esp_reset_reason_t rr) {
  prefs.begin("solardiag", false);
  g_resetCount = prefs.getUInt("rstcnt", 0) + 1;
  prefs.putUInt("rstcnt", g_resetCount);
  prefs.end();
  snprintf(g_diagA, sizeof(g_diagA), "rst=%s n=%lu", resetReasonName(rr), (unsigned long)g_resetCount);
  snprintf(g_diagB, sizeof(g_diagB), "nodump");
  if (esp_core_dump_image_check() == ESP_OK) {
    esp_core_dump_summary_t *cd = (esp_core_dump_summary_t *)malloc(sizeof(*cd));
    if (cd) {
      if (esp_core_dump_get_summary(cd) == ESP_OK) {
        snprintf(g_diagA, sizeof(g_diagA), "rst=%s n=%lu t=%.10s",
                 resetReasonName(rr), (unsigned long)g_resetCount, cd->exc_task);
        snprintf(g_diagB, sizeof(g_diagB), "pc=%08lX mc=%lu s=%.8s",
                 (unsigned long)cd->exc_pc, (unsigned long)cd->ex_info.mcause, (const char *)cd->app_elf_sha256);
      } else {
        snprintf(g_diagB, sizeof(g_diagB), "dump=unreadable");
      }
      free(cd);
    } else {
      snprintf(g_diagB, sizeof(g_diagB), "dump=nomem");
    }
  }
  Serial.printf("DIAG %s | %s\n", g_diagA, g_diagB);
}

static void onDiagCrashWrite(float value) {
  if (value != DIAG_CRASH_MAGIC) return;
  Serial.println("DIAG: crash test requested — panicking in 200 ms");
  delay(200);
  volatile uint32_t *nowhere = (uint32_t *)0x00000000;
  *nowhere = 0xDEADBEEF;
}

/* ============================================================
 *  SENSOR HELPERS
 * ============================================================ */
static int cmpAddr(const void *a, const void *b) { return memcmp(a, b, sizeof(DeviceAddress)); }

/* Enumerate the DS18B20s and sort by ROM so EP10/11/12 are STABLE across reboots. Which physical
 * probe is which is decided at commissioning (warm one, watch Z2M) and recorded in the converter's
 * probe_roles option — the firmware does not need to know. */
static void discoverProbes() {
  ds.begin();
  ds.setResolution(12);
  ds.setWaitForConversion(false);
  probeCount = 0;
  DeviceAddress a;
  oneWire.reset_search();
  while (probeCount < 3 && oneWire.search(a)) {
    if (OneWire::crc8(a, 7) == a[7] && a[0] == 0x28) memcpy(probeAddr[probeCount++], a, 8);
  }
  qsort(probeAddr, probeCount, sizeof(DeviceAddress), cmpAddr);
  Serial.printf("DS18B20: %u probe(s)\n", probeCount);
  for (uint8_t i = 0; i < probeCount; i++) {
    Serial.printf("  EP%u ROM ", 10 + i);
    for (int k = 0; k < 8; k++) Serial.printf("%02X", probeAddr[i][k]);
    Serial.println();
  }
}

/* True-RMS current on one differential pair over CT_SAMPLE_MS (~20 mains cycles). The CT sits on a
 * VDD/2 bias, so the ADS reading is a signed swing around ~0 differential; the mean is removed anyway. */
static float readCurrentRMS(uint8_t pair) {
  if (!adsOk) return NAN;
  double sum = 0, sumSq = 0; uint32_t n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < CT_SAMPLE_MS) {
    int16_t raw = (pair == 0) ? ads.readADC_Differential_0_1() : ads.readADC_Differential_2_3();
    double amps = (raw * ADS_MV_PER_BIT / 1000.0) * CT_FACTOR;
    sum += amps; sumSq += amps * amps; n++;
  }
  if (n == 0) return NAN;
  double mean = sum / n;
  double var = sumSq / n - mean * mean;
  return var > 0 ? (float)sqrt(var) : 0.0f;
}

/* Disable the stack's own reporting on a temperature endpoint (see the wall module v10 notes): a
 * stale periodic report config left in NVS by a coordinator crashes this zboss build. Leaves the
 * table itself intact, which the manual-report path needs. */
static void neutraliseTempReporting(uint8_t ep) {
  esp_zb_zcl_reporting_info_t ri = {};
  ri.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV; ri.ep = ep;
  ri.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT; ri.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  ri.attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID; ri.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  ri.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  ri.u.send_info.min_interval = ri.u.send_info.max_interval = 0xFFFF;
  ri.u.send_info.def_min_interval = ri.u.send_info.def_max_interval = 0xFFFF;
  ri.u.send_info.delta.s16 = 0x7FFF;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t r = esp_zb_zcl_update_reporting_info(&ri);
  esp_zb_lock_release();
  Serial.printf("EP%u temp reporting neutralised: %s\n", ep, esp_err_to_name(r));
}

void triggerFactoryReset() {
  Serial.println("FACTORY RESET TRIGGERED!");
#ifdef HAS_STATUS_LED
  for (int i = 0; i < 6; i++) { digitalWrite(STATUS_LED, LOW); delay(150); digitalWrite(STATUS_LED, HIGH); delay(150); }
#endif
  Zigbee.factoryReset();
}

/* ============================================================ */
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef HAS_STATUS_LED
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, LOW);   // LED ON = booting
#endif

  // 1. Reset diagnostics
  prefs.begin("solarcfg", false);
  g_brownoutCount = prefs.getUInt("bodcnt", 0);
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_BROWNOUT) { g_brownoutCount++; prefs.putUInt("bodcnt", g_brownoutCount); }
  prefs.end();
  Serial.printf("Boot: reset_reason=%d brownout_count=%u\n", (int)rr, g_brownoutCount);
  buildDiagSummary(rr);

  // 2. Sensors
  discoverProbes();
  Wire.begin(I2C_SDA, I2C_SCL);
  adsOk = ads.begin(0x48, &Wire);
  if (adsOk) { ads.setGain(ADS_GAIN); ads.setDataRate(RATE_ADS1115_860SPS); }
  Serial.printf("ADS1115: %s\n", adsOk ? "ok" : "NOT FOUND");

  // 3. Zigbee endpoints
  for (int i = 0; i < 3; i++) {
    zbProbe[i].setManufacturerAndModel("DIY", "ESP32C6-SOLAR-MON");
    zbProbe[i].setMinMaxValue(-40, 125);
  }
  zbDie.setManufacturerAndModel("DIY", "ESP32C6-SOLAR-MON");
  zbDie.setMinMaxValue(-40, 125);
  zbHeater.setManufacturerAndModel("DIY", "ESP32C6-SOLAR-MON");
  zbHeater.addAnalogInput();
  zbHeater.setAnalogInputDescription("Heater current A");
  zbCirc.setManufacturerAndModel("DIY", "ESP32C6-SOLAR-MON");
  zbCirc.addAnalogInput();
  zbCirc.setAnalogInputDescription("Circulator current A");
  zbDiag.setManufacturerAndModel("DIY", "ESP32C6-SOLAR-MON");
  zbDiag.addAnalogInput();
  zbDiag.setAnalogInputDescription(g_diagA);      // written ONCE before begin() — ZBOSS keeps the pointer
  zbDiag.addAnalogOutput();
  zbDiag.setAnalogOutputDescription(g_diagB);
  zbDiag.onAnalogOutputChange(onDiagCrashWrite);

  zbProbe[0].addOTAClient(OTA_FW_RUNNING, OTA_FW_DOWNLOADED, OTA_HW_VERSION, OTA_MANUFACTURER, OTA_IMAGE_TYPE);

  for (int i = 0; i < 3; i++) Zigbee.addEndpoint(&zbProbe[i]);
  Zigbee.addEndpoint(&zbDie);
  Zigbee.addEndpoint(&zbHeater);
  Zigbee.addEndpoint(&zbCirc);
  Zigbee.addEndpoint(&zbDiag);

  Zigbee.begin(ZIGBEE_ROUTER);
  for (uint8_t ep = 10; ep <= 13; ep++) neutraliseTempReporting(ep);

  Serial.println("Joining...");
  unsigned long t0 = millis();
  while (!Zigbee.connected()) {
#ifdef HAS_STATUS_LED
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
#endif
    delay(250);
    if (millis() - t0 > 300000UL) { Serial.println("Join timeout — continuing, will retry in background"); break; }
  }
#ifdef HAS_STATUS_LED
  digitalWrite(STATUS_LED, HIGH);   // off = running
#endif
}

/* ============================================================ */
void loop() {
  unsigned long now = millis();

  // --- post-connect one-shots
  static bool otaRequested = false;
  if (!otaRequested && Zigbee.connected()) { otaRequested = true; zbProbe[0].requestOTAUpdate(); }
  if (!diagReported && Zigbee.connected() && now > 4000) {
    diagReported = true;
    zbDiag.setAnalogInput((float)g_brownoutCount);   // value only — NEVER reportAnalogInput()
  }
  static bool imgValidated = false;
  if (!imgValidated && Zigbee.connected() && now > OTA_VALIDATE_MS) {
    imgValidated = true;
    const esp_partition_t *run = esp_ota_get_running_partition(); esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback(); Serial.println("OTA image confirmed healthy");
    }
  }

  // --- measurement cycle: request DS conversion, sample CTs meanwhile, then read probes
  static unsigned long lastMeasure = 0 - MEASURE_MS, dsRequestedAt = 0;
  static bool dsPending = false;
  if (!dsPending && now - lastMeasure >= MEASURE_MS) {
    lastMeasure = now;
    if (probeCount) { ds.requestTemperatures(); dsRequestedAt = now; dsPending = true; }
    float ih = readCurrentRMS(0), ic = readCurrentRMS(1);
    Serial.printf("CT heater=%.3f A circ=%.3f A\n", ih, ic);
    if (Zigbee.connected()) {
      if (!isnan(ih)) zbHeater.setAnalogInput(ih);
      if (!isnan(ic)) zbCirc.setAnalogInput(ic);
      zbDie.setTemperature(temperatureRead());
    }
  }
  if (dsPending && now - dsRequestedAt >= DS_CONVERT_MS) {
    dsPending = false;
    for (uint8_t i = 0; i < probeCount; i++) {
      float t = ds.getTempC(probeAddr[i]);
      if (t == DEVICE_DISCONNECTED_C || t < -50 || t > 130) { Serial.printf("EP%u probe read failed\n", 10 + i); continue; }
      Serial.printf("EP%u %.2f C\n", 10 + i, t);
      if (Zigbee.connected()) zbProbe[i].setTemperature(t);
    }
  }

  // --- factory-reset button (IO9 / BOOT): hold 5 s
  bool b = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && b == LOW) { buttonPressed = true; buttonPressTime = now; }
  if (lastButtonState == LOW && b == HIGH) buttonPressed = false;
  if (buttonPressed && now - buttonPressTime > 5000) { buttonPressed = false; triggerFactoryReset(); }
  lastButtonState = b;

  delay(20);
}
