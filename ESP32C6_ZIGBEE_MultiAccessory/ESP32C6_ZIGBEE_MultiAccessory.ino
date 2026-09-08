#include "Zigbee.h"
#include <Wire.h>
#include <Preferences.h>
#include "esp_system.h"    // esp_reset_reason()
#include "esp_ota_ops.h"   // OTA rollback: mark-valid / running-partition state
#include <ir_LG.h>          // IRremoteESP8266 — LG split control (proven on the C6 by the HomeKit firmware)

/* OTA ANTI-BRICK ROLLBACK — verifyRollbackLater() is defined below, returning
 * true so the arduino-esp32 core does NOT auto-confirm a freshly-OTA'd image;
 * loop() confirms it only after OTA_VALIDATE_MS of healthy joined uptime. */

/*
 * ESP32-C6 ZIGBEE MULTIACCESSORY  (v5, 0x01000003)  — T-0036
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
 * v3 adds the AC: the LG split is driven over IR and presented to Z2M/HA as a
 * proper climate device. There is no thermostat SERVER in the Arduino Zigbee
 * library (ZigbeeThermostat is a CLIENT, for building a wall thermostat), so the
 * AC is COMPOSED from primitives that are already proven on this stack — a
 * multistate output for the mode, an analog output for the setpoint, a real
 * FanControl endpoint, and a binary output for the swing — and the external
 * converter assembles them into one `climate` expose. HA therefore still gets a
 * genuine climate entity, and HomeKit a thermostat, with no raw esp_zb work.
 * IR is fire-and-forget, so the state is optimistic (same as the old firmware).
 * `local_temperature` comes from the AM2320 via the converter — an upgrade on the
 * HomeKit firmware, where the AC had no idea of the room temperature.
 *
 * v2 SCOPE: ALL FIVE OUTPUT PROFILES + a transition ramp. Everything is proven
 * on the bench board before any installed board is touched, so the profiles the
 * current fleet does not use (2XCCT / RGBW / RGB_DIM) are implemented too rather
 * than deferred — the bench board is the only place they can be tested, and once
 * a board is in a wall nobody wants to iterate on it. The composed AC endpoint
 * (phase 3) is still to come.
 *
 * ENDPOINT MAP
 *   EP10..EP13  Dimmable Light, one per PWM channel (OTA client sits on EP10)
 *   EP14        Multistate Output -> output profile selector
 *   EP15        Analog Input      -> brownout/reset counter
 *   EP16        Temperature       -> C6 die temperature (diagnostic)
 *   EP18        Binary Output     -> AC ENABLED (config; IR can't be probed)
 *   EP20        Temperature + Humidity -> AM2320, ONLY IF DETECTED
 *   EP30        Multistate Output -> AC system mode (off/cool/heat/dry/fan_only)
 *   EP31        Analog Output     -> AC setpoint °C (16..30)
 *   EP32        Fan Control       -> AC fan mode
 *   EP33        Binary Output     -> AC vertical swing
 *   (EP30..EP33 exist only when AC ENABLED)
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
/* Channel -> GPIO, and which LED each one actually drives.
 *
 * MEASURED ON THE HARDWARE 2026-09-08 with an RGBW strip connected, driving one
 * channel at a time. This supersedes BOTH earlier sources, which were wrong:
 *
 *   endpoint  GPIO   ACTUAL colour     silkscreen said   legacy config said
 *   l1/CH1    IO6    BLUE              W                 BLUE
 *   l2/CH2    IO5    WHITE             B                 GREEN
 *   l3/CH3    IO4    GREEN             R                 RED
 *   l4/CH4    IO2    RED               G                 WHITE
 *
 * i.e. relative to the silkscreen the pairs are transposed (G<->R and B<->W).
 * Reading the labels off the board made green come out red. Do NOT "fix" this
 * back to the silkscreen order without re-measuring: label order and driver
 * order do not agree on this board.
 *
 * The CCT pairs are unaffected — a CCT strip just needs two channels, and pair 2
 * (CH3+CH4 = IO4+IO2, the terminals marked G and R) is confirmed working.
 */
static const uint8_t CH_PIN[4] = { 6, 5, 4, 2 };  // CH1..CH4  (IO6, IO5, IO4, IO2)
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
/* The AM2320 needs ~2 s after power-up before it answers at all, so a probe run
 * the instant we boot reports "absent" on a board that has one — and EP20 is
 * decided at boot, so that mistake sticks until the next reboot + re-interview.
 * Probe across a window wider than the sensor's wake time. */
#define AM2320_PROBE_ATTEMPTS 8
#define AM2320_PROBE_GAP_MS   400
/* A profile write must be ACKNOWLEDGED before we reboot: rebooting inside the
 * ZCL callback means the stack never sends the write response and the
 * coordinator logs the write as failed even though it succeeded. */
#define PROFILE_REBOOT_DELAY_MS 2500UL
/* Transition ramp. The Zigbee level-control transition time is not surfaced by
 * the Arduino endpoint classes, so a bare command is a step change — visibly
 * worse than the HomeKit firmware it replaces. A short linear ramp fixes that.
 * Kept SHORT on purpose: one of the target boards drives motion-triggered stair
 * lighting, where a slow fade reads as lag. This is a fade-in, not a delay —
 * the light starts moving immediately. */
#define RAMP_MS 250UL
/* Mired range advertised for CCT endpoints: 153 = 6500 K (cool), 500 = 2000 K
 * (warm). Z2M/HA send mireds; the firmware mixes cool/warm to match. */
#define MIRED_MIN 153
#define MIRED_MAX 500
#define CHMASK_DEFAULT 0x0F   // all four channels populated
/* HA/Z2M often writes mode + setpoint + fan in one go. Coalesce them into ONE IR
 * frame instead of blasting the split three times. */
#define AC_SEND_DEBOUNCE_MS 400UL
#define AC_SETPOINT_MIN 16
#define AC_SETPOINT_MAX 30
enum AcMode : uint16_t {
  AC_OFF = 0, AC_COOL = 1, AC_HEAT = 2, AC_DRY = 3, AC_FAN_ONLY = 4, AC_MODE_COUNT = 5
};
static const char *AC_MODE_NAME[AC_MODE_COUNT] = {"off", "cool", "heat", "dry", "fan_only"};

/* OTA identity. NOTE image type 0x1012 — distinct from the wall-input module's
 * 0x1011 so the two DIY devices never see each other's images in the Z2M
 * ota_override index. BUMP OTA_FW_RUNNING EVERY RELEASE (Z2M only offers a
 * strictly-higher fileVersion). */
#define OTA_FW_RUNNING     0x01000003
#define OTA_FW_DOWNLOADED  0x01000004
#define OTA_HW_VERSION     0x0101
#define OTA_MANUFACTURER   0x1001
#define OTA_IMAGE_TYPE     0x1012

#define ZB_MANUFACTURER "DIY"
#define ZB_MODEL        "ESP32C6-MULTIACCESSORY"

/* ============================================================
 *  OUTPUT PROFILES
 * ============================================================ */
enum Profile : uint16_t {
  PROFILE_4XDIM    = 0,  // 4 independent dimmers
  PROFILE_CCT_2DIM = 1,  // CCT on CH1+CH2, dimmers on CH3, CH4
  PROFILE_2XCCT    = 2,  // CCT on CH1+CH2 and CH3+CH4
  PROFILE_RGBW     = 3,  // R=IO2 G=IO4 B=IO6 W=IO5 (measured, not the silkscreen)
  PROFILE_RGB_DIM  = 4,  // RGB as above, dimmer on the spare white channel (IO5)
  PROFILE_COUNT    = 5
};
static const char *PROFILE_NAME[PROFILE_COUNT] = {
  "4XDIM", "CCT_2DIM", "2XCCT", "RGBW", "RGB_DIM"
};

/* Which profiles this build can actually run — all of them as of v2. A write of
 * anything outside this set is REJECTED rather than stored: accepting it would
 * reboot the board into a profile it cannot honour and leave Z2M showing a
 * profile the device isn't running. */
static bool profileImplemented(uint16_t p) {
  return p < PROFILE_COUNT;
}

static Profile   g_profile        = PROFILE_4XDIM;   // effective (what is running)
static uint16_t  g_profileStored  = PROFILE_4XDIM;   // what NVS says
static uint32_t  g_brownoutCount  = 0;
static bool      g_haveAM2320     = false;
static bool      g_imgValidated   = false;
static bool      g_rebootPending  = false;
static unsigned long g_rebootAt   = 0;
/* setMultistateOutput() re-enters onMultistateOutputChange(), so "bouncing" the
 * attribute back to the running profile re-triggers the handler. If the stored
 * profile is itself unimplemented that becomes an endless reject/bounce loop
 * (observed on the bench). One-shot flag to swallow our own echo. */
static bool      g_profileEcho    = false;
static uint8_t   g_chMask         = CHMASK_DEFAULT;
static bool      g_maskEcho       = false;
static bool      g_acEnabled      = false;   // config: is an LG split wired to the IR LED?
static bool      g_acCfgEcho      = false;
static uint16_t  g_acMode         = AC_OFF;
static float     g_acSetpoint     = 24.0f;
static uint8_t   g_acFan          = FAN_MODE_AUTO;
static bool      g_acSwing        = false;
static bool      g_acDirty        = false;
static unsigned long g_acSendAt   = 0;

Preferences prefs;

/* ============================================================
 *  ENDPOINTS
 * ------------------------------------------------------------
 *  The light endpoints depend on the profile, and C++ globals are constructed
 *  unconditionally, so they are built with `new` in setup() once the profile is
 *  known. Fixed endpoints (config/diagnostics/sensor) stay static.
 * ============================================================ */
ZigbeeDimmableLight      *zbDim[4] = { nullptr, nullptr, nullptr, nullptr };  // slot -> dimmer
ZigbeeColorDimmableLight *zbCct[2] = { nullptr, nullptr };                    // slot -> CCT pair
ZigbeeColorDimmableLight *zbRgb    = nullptr;                                 // RGB(W)
ZigbeeEP                 *zbEp10   = nullptr;   // whichever endpoint owns EP10 (carries OTA)

ZigbeeMultistate  zbCfg(14);    // profile selector
ZigbeeAnalog      zbDiag(15);   // brownout counter
ZigbeeTempSensor  zbDie(16);    // die temperature
ZigbeeAnalog      zbMask(17);   // channel mask (which channels are populated)
ZigbeeBinary      zbAcCfg(18);  // config: AC enabled
ZigbeeMultistate  zbAcMode(30); // AC system mode      (created only when enabled)
ZigbeeAnalog      zbAcTemp(31); // AC setpoint
ZigbeeFanControl  zbAcFan(32);  // AC fan mode
ZigbeeBinary      zbAcSwing(33);// AC vertical swing
IRLgAc           *g_ir = nullptr;
ZigbeeTempSensor  zbRoom(20);   // AM2320 (added only when present)

/* Channel mapping for the running profile. 0xFF = unused. */
static uint8_t g_dimCh[4]  = {0xFF, 0xFF, 0xFF, 0xFF};   // dimmer slot -> channel
static uint8_t g_cctCool[2] = {0xFF, 0xFF};
static uint8_t g_cctWarm[2] = {0xFF, 0xFF};
static uint8_t g_rgbR = 0xFF, g_rgbG = 0xFF, g_rgbB = 0xFF, g_rgbW = 0xFF;

/* ============================================================
 *  PWM OUTPUT + TRANSITION RAMP
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

/* Each channel ramps linearly from where it was to where it is going, so a
 * command is a fast fade rather than a step. stepRamps() runs from loop(). */
struct ChannelRamp {
  uint16_t from;
  uint16_t to;
  uint16_t cur;
  unsigned long t0;
};
static ChannelRamp g_ramp[4];

/* The single seam every output write goes through. */
static void applyDuty(uint8_t ch, uint32_t duty) {
  if (ch >= 4) return;
  if (duty > DUTY_MAX) duty = DUTY_MAX;
  ChannelRamp &r = g_ramp[ch];
  if (r.to == duty) return;
  r.from = r.cur;
  r.to = (uint16_t)duty;
  r.t0 = millis();
}

static void stepRamps() {
  const unsigned long now = millis();
  for (uint8_t ch = 0; ch < 4; ch++) {
    ChannelRamp &r = g_ramp[ch];
    if (r.cur == r.to) continue;
    const unsigned long elapsed = now - r.t0;
    uint16_t next;
    if (elapsed >= RAMP_MS) {
      next = r.to;
    } else {
      const int32_t span = (int32_t)r.to - (int32_t)r.from;
      next = (uint16_t)((int32_t)r.from + (span * (int32_t)elapsed) / (int32_t)RAMP_MS);
    }
    if (next != r.cur) {
      r.cur = next;
      ledcWrite(CH_PIN[ch], r.cur);
    }
  }
}

/* ---- dimmer ---- */
static void setDimmer(uint8_t slot, bool state, uint8_t level) {
  const uint8_t ch = g_dimCh[slot];
  if (ch == 0xFF) return;
  Serial.printf("DIM%u (CH%u) -> %s level=%u\n", slot + 1, ch + 1, state ? "ON" : "OFF", level);
  applyDuty(ch, state ? levelToDuty(level) : 0);
}

/* ---- CCT: mireds -> cool/warm mix ----
 * ratio 0 = fully cool, 1 = fully warm. The two duties sum to the requested
 * level, so perceived output stays roughly constant as colour temperature
 * moves — mixing at full duty on both would jump brighter mid-range. */
static void setCct(uint8_t slot, bool state, uint8_t level, uint16_t mireds) {
  const uint8_t cool = g_cctCool[slot], warm = g_cctWarm[slot];
  if (cool == 0xFF || warm == 0xFF) return;
  if (mireds < MIRED_MIN) mireds = MIRED_MIN;
  if (mireds > MIRED_MAX) mireds = MIRED_MAX;
  const float ratio = (float)(mireds - MIRED_MIN) / (float)(MIRED_MAX - MIRED_MIN);
  const uint32_t duty = state ? levelToDuty(level) : 0;
  Serial.printf("CCT%u (CH%u/CH%u) -> %s level=%u mireds=%u (warm %.0f%%)\n",
                slot + 1, cool + 1, warm + 1, state ? "ON" : "OFF", level, mireds, ratio * 100.0f);
  applyDuty(cool, (uint32_t)(duty * (1.0f - ratio)));
  applyDuty(warm, (uint32_t)(duty * ratio));
}

/* ---- RGB(W) ----
 * Classic RGBW conversion: the common part of R/G/B is what a dedicated white
 * channel does better (higher CRI, more output, less power), so pull it out and
 * drive W with it. On RGB_DIM there is no W channel and the colour is left as
 * sent. */
/* moveToColor writes currentX and currentY as SEPARATE attributes, and the
 * endpoint class fires the change callback after each one — so the first call
 * computes a colour from the new x with the STALE y and briefly drives a wrong
 * colour (bench: a green target flashed cyan before settling). Hold the latest
 * request for a few ms and apply once things have settled. */
#define RGB_COALESCE_MS 60UL
static bool     g_rgbPending = false;
static unsigned long g_rgbAt = 0;
static bool     g_rgbState = false;
static uint8_t  g_rgbR8 = 0, g_rgbG8 = 0, g_rgbB8 = 0, g_rgbLevel = 0;

static void applyRgbNow();

static void setRgb(bool state, uint8_t r, uint8_t g, uint8_t b, uint8_t level) {
  g_rgbState = state; g_rgbR8 = r; g_rgbG8 = g; g_rgbB8 = b; g_rgbLevel = level;
  g_rgbAt = millis() + RGB_COALESCE_MS;
  g_rgbPending = true;
}

static void applyRgbNow() {
  g_rgbPending = false;
  const bool state = g_rgbState;
  uint8_t r = g_rgbR8, g = g_rgbG8, b = g_rgbB8;
  const uint8_t level = g_rgbLevel;
  const float scale = state ? (float)levelToDuty(level) / (float)DUTY_MAX : 0.0f;
  uint8_t w = 0;
  if (g_rgbW != 0xFF) {
    w = r < g ? (r < b ? r : b) : (g < b ? g : b);   // min(r,g,b)
    r -= w; g -= w; b -= w;
  }
  Serial.printf("RGB%s -> %s r=%u g=%u b=%u%s level=%u\n",
                g_rgbW != 0xFF ? "W" : "", state ? "ON" : "OFF", r, g, b,
                g_rgbW != 0xFF ? "" : " (no W)", level);
  if (g_rgbR != 0xFF) applyDuty(g_rgbR, (uint32_t)(r * scale * DUTY_MAX / 255.0f));
  if (g_rgbG != 0xFF) applyDuty(g_rgbG, (uint32_t)(g * scale * DUTY_MAX / 255.0f));
  if (g_rgbB != 0xFF) applyDuty(g_rgbB, (uint32_t)(b * scale * DUTY_MAX / 255.0f));
  if (g_rgbW != 0xFF) applyDuty(g_rgbW, (uint32_t)(w * scale * DUTY_MAX / 255.0f));
}

/* The endpoint callbacks are bare function pointers with no user context, so
 * one thunk per slot. */
static void onDim0(bool s, uint8_t l) { setDimmer(0, s, l); }
static void onDim1(bool s, uint8_t l) { setDimmer(1, s, l); }
static void onDim2(bool s, uint8_t l) { setDimmer(2, s, l); }
static void onDim3(bool s, uint8_t l) { setDimmer(3, s, l); }
static void onCct0(bool s, uint8_t l, uint16_t t) { setCct(0, s, l, t); }
static void onCct1(bool s, uint8_t l, uint16_t t) { setCct(1, s, l, t); }
static void onRgbCb(bool s, uint8_t r, uint8_t g, uint8_t b, uint8_t l) { setRgb(s, r, g, b, l); }

/* In HUE/SATURATION colour mode the endpoint class calls lightChangedHsv(), NOT
 * the RGB callback — registering only the RGB one means commands arrive and
 * nothing reaches the pins. Convert here and reuse the RGB path so the white
 * extraction and level scaling stay in one place. Zigbee hue/sat are 0..254. */
static void onHsvCb(bool state, uint8_t hue, uint8_t sat, uint8_t val) {
  const float H = (float)hue * 360.0f / 254.0f;
  const float S = (float)sat / 254.0f;
  const float C = S;                       // value is carried by `val` (the level)
  const float X = C * (1.0f - fabsf(fmodf(H / 60.0f, 2.0f) - 1.0f));
  const float m = 1.0f - C;
  float r1, g1, b1;
  if      (H <  60) { r1 = C; g1 = X; b1 = 0; }
  else if (H < 120) { r1 = X; g1 = C; b1 = 0; }
  else if (H < 180) { r1 = 0; g1 = C; b1 = X; }
  else if (H < 240) { r1 = 0; g1 = X; b1 = C; }
  else if (H < 300) { r1 = X; g1 = 0; b1 = C; }
  else              { r1 = C; g1 = 0; b1 = X; }
  setRgb(state, (uint8_t)((r1 + m) * 255.0f + 0.5f),
                (uint8_t)((g1 + m) * 255.0f + 0.5f),
                (uint8_t)((b1 + m) * 255.0f + 0.5f), val);
}

/* ============================================================
 *  PROFILE SELECTOR (EP14)
 * ============================================================ */
/* Put the attribute back to what the device is really running, so the Z2M UI
 * cannot show a profile the firmware isn't honouring. */
static void bounceProfileAttr() {
  g_profileEcho = true;
  zbCfg.setMultistateOutput(g_profileStored);
}

static void onProfileWrite(uint16_t state) {
  if (g_profileEcho) { g_profileEcho = false; return; }   // our own bounce, not a user write
  if (state >= PROFILE_COUNT) {
    Serial.printf("Profile %u out of range, ignored\n", state);
    bounceProfileAttr();
    return;
  }
  if (!profileImplemented(state)) {
    Serial.printf("Profile %s not implemented in this build — REJECTED (still %s)\n",
                  PROFILE_NAME[state], PROFILE_NAME[g_profileStored]);
    bounceProfileAttr();
    return;
  }
  if (state == g_profileStored) return;

  prefs.begin("macfg", false);
  prefs.putUShort("profile", state);
  prefs.end();
  g_profileStored = state;

  /* Endpoint composition is fixed at interview time, so a profile change only
   * takes effect after a reboot — and Z2M must re-interview afterwards to pick
   * up the new endpoints. Defer the restart to loop() so the stack can finish
   * answering this write first; rebooting from inside the callback makes the
   * coordinator log a perfectly good write as a timeout. */
  g_rebootAt = millis() + PROFILE_REBOOT_DELAY_MS;
  g_rebootPending = true;
  Serial.printf("Profile -> %s, rebooting in %lu ms (re-interview in Z2M after)\n",
                PROFILE_NAME[state], PROFILE_REBOOT_DELAY_MS);
}

static void onMaskWrite(float value) {
  if (g_maskEcho) { g_maskEcho = false; return; }        // our own bounce
  const int v = (int)(value + 0.5f);
  if (v < 1 || v > 0x0F) {
    // 0 would leave the board with no lights at all, and nothing above 0b1111
    // maps to a channel. Refuse and put the real value back.
    Serial.printf("Channel mask %d invalid (1..15) — rejected\n", v);
    g_maskEcho = true;
    zbMask.setAnalogOutput((float)g_chMask);
    return;
  }
  if ((uint8_t)v == g_chMask) return;

  prefs.begin("macfg", false);
  prefs.putUChar("chmask", (uint8_t)v);
  prefs.end();
  g_chMask = (uint8_t)v;

  // Same rule as the profile: endpoint composition is fixed at interview time,
  // so this needs a reboot, and Z2M must re-interview afterwards. Deferred to
  // loop() so the write is acknowledged first.
  g_rebootAt = millis() + PROFILE_REBOOT_DELAY_MS;
  g_rebootPending = true;
  Serial.printf("Channel mask -> 0b%c%c%c%c, rebooting in %lu ms (re-interview in Z2M after)\n",
                (v & 8) ? '1' : '0', (v & 4) ? '1' : '0',
                (v & 2) ? '1' : '0', (v & 1) ? '1' : '0', PROFILE_REBOOT_DELAY_MS);
}

/* ============================================================
 *  AC (LG split over IR)
 * ------------------------------------------------------------
 *  Composed from a multistate output (mode), an analog output (setpoint), a
 *  FanControl endpoint and a binary output (swing) — see the header for why.
 *  IR is one-way, so state is optimistic: whatever was last commanded is what
 *  we believe the split is doing.
 * ============================================================ */
static uint8_t acFanToLg(uint8_t fanMode) {
  switch (fanMode) {
    case FAN_MODE_OFF:
    case FAN_MODE_LOW:    return kLgAcFanLow;
    case FAN_MODE_MEDIUM: return kLgAcFanMedium;
    case FAN_MODE_HIGH:   return kLgAcFanHigh;
    default:              return kLgAcFanAuto;   // ON / AUTO / SMART
  }
}

/* Queue an IR frame. Several attribute writes usually arrive together (HA sets
 * mode, setpoint and fan in one service call), so they coalesce into one send. */
static void acTouch() {
  g_acDirty = true;
  g_acSendAt = millis() + AC_SEND_DEBOUNCE_MS;
}

static void acSendNow() {
  g_acDirty = false;
  if (!g_ir) return;
  if (g_acMode == AC_OFF) {
    g_ir->off();
    Serial.println("AC -> OFF (IR sent)");
  } else {
    g_ir->on();
    switch (g_acMode) {
      case AC_COOL:     g_ir->setMode(kLgAcCool); break;
      case AC_HEAT:     g_ir->setMode(kLgAcHeat); break;
      case AC_DRY:      g_ir->setMode(kLgAcDry);  break;
      case AC_FAN_ONLY: g_ir->setMode(kLgAcFan);  break;
      default: break;
    }
    int t = (int)(g_acSetpoint + 0.5f);
    if (t < kLgAcMinTemp) t = kLgAcMinTemp;
    if (t > kLgAcMaxTemp) t = kLgAcMaxTemp;
    g_ir->setTemp(t);
    g_ir->setFan(acFanToLg(g_acFan));
    g_ir->setSwingV(g_acSwing);
    Serial.printf("AC -> %s %d C fan=%u swing=%u (IR sent)\n",
                  AC_MODE_NAME[g_acMode], t, g_acFan, g_acSwing ? 1 : 0);
  }
  g_ir->send();
}

static void onAcModeWrite(uint16_t state) {
  if (state >= AC_MODE_COUNT) { zbAcMode.setMultistateOutput(g_acMode); return; }
  g_acMode = state;
  acTouch();
}

static void onAcTempWrite(float value) {
  if (value < AC_SETPOINT_MIN) value = AC_SETPOINT_MIN;
  if (value > AC_SETPOINT_MAX) value = AC_SETPOINT_MAX;
  g_acSetpoint = value;
  acTouch();
}

static void onAcFanWrite(ZigbeeFanMode mode) {
  g_acFan = (uint8_t)mode;
  acTouch();
}

static void onAcSwingWrite(bool on) {
  g_acSwing = on;
  acTouch();
}

/* Config: is a split actually wired to the IR LED? Can't be probed (IR is
 * output-only), so it is a stored flag, applied on reboot like the profile. */
static void onAcCfgWrite(bool enabled) {
  if (g_acCfgEcho) { g_acCfgEcho = false; return; }
  if (enabled == g_acEnabled) return;
  prefs.begin("macfg", false);
  prefs.putBool("ac", enabled);
  prefs.end();
  g_acEnabled = enabled;
  g_rebootAt = millis() + PROFILE_REBOOT_DELAY_MS;
  g_rebootPending = true;
  Serial.printf("AC %s, rebooting in %lu ms (re-interview in Z2M after)\n",
                enabled ? "ENABLED" : "disabled", PROFILE_REBOOT_DELAY_MS);
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
  for (uint8_t attempt = 0; attempt < AM2320_PROBE_ATTEMPTS; attempt++) {
    Wire.beginTransmission(AM2320_ADDR);
    Wire.endTransmission();          // wake-up (expected to NAK)
    delay(3);
    Wire.beginTransmission(AM2320_ADDR);
    if (Wire.endTransmission() == 0) return true;
    delay(AM2320_PROBE_GAP_MS);
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
  g_chMask = prefs.getUChar("chmask", CHMASK_DEFAULT);
  g_acEnabled = prefs.getBool("ac", false);
  if (g_chMask < 1 || g_chMask > 0x0F) g_chMask = CHMASK_DEFAULT;   // self-heal
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_BROWNOUT) { g_brownoutCount++; prefs.putUInt("bodcnt", g_brownoutCount); }
  prefs.end();

  g_profile = (Profile)g_profileStored;
  if (g_profileStored >= PROFILE_COUNT || !profileImplemented(g_profileStored)) {
    // Never brick a board over a config value: run what we can, and SELF-HEAL the
    // stored value so it always names a runnable profile. Without this, a stored
    // profile this build can't honour makes the reject/bounce path fight itself
    // and leaves Z2M advertising a profile the device isn't running.
    Serial.printf("Stored profile %s not implemented in this build — falling back to 4XDIM and correcting NVS\n",
                  g_profileStored < PROFILE_COUNT ? PROFILE_NAME[g_profileStored] : "?");
    g_profile = PROFILE_4XDIM;
    g_profileStored = PROFILE_4XDIM;
    prefs.begin("macfg", false);
    prefs.putUShort("profile", PROFILE_4XDIM);
    prefs.end();
  }
  Serial.printf("Firmware 0x%08lX\n", (unsigned long)OTA_FW_RUNNING);
  Serial.printf("Boot: reset_reason=%d brownout_count=%u profile=%s(stored %u) chmask=0b%c%c%c%c\n",
                (int)rr, g_brownoutCount, PROFILE_NAME[g_profile], g_profileStored,
                (g_chMask & 8) ? '1' : '0', (g_chMask & 4) ? '1' : '0',
                (g_chMask & 2) ? '1' : '0', (g_chMask & 1) ? '1' : '0');
  Serial.printf("AC: %s\n", g_acEnabled ? "enabled -> EP30..EP33" : "disabled (set 'ac_enabled' in Z2M if a split is wired)");

  // 3. I2C + sensor auto-detect. EP20 exists only if the AM2320 answers, so
  //    "has a sensor" never has to be configured.
  Wire.begin(SDA_PIN, SCL_PIN);
  g_haveAM2320 = am2320Probe();
  Serial.printf("AM2320: %s\n", g_haveAM2320
    ? "detected -> EP20"
    : "absent (EP20 omitted; if a sensor is fitted, reboot + re-interview in Z2M)");

  // 4. Light endpoints for the running profile. Endpoint NUMBERS are reserved by
  //    role (10..13) so a given slot always lands on the same endpoint no matter
  //    which profile is running — a CCT light on EP10 replaces the dimmer that
  //    would otherwise be there, and EP11 simply does not exist because CH2 is
  //    its warm half.
  const auto chOn  = [&](uint8_t ch) { return (g_chMask >> ch) & 1; };
  const auto pairOn = [&](uint8_t a, uint8_t b) { return chOn(a) && chOn(b); };

  auto newDim = [&](uint8_t slot, uint8_t endpoint, uint8_t ch) {
    if (!chOn(ch)) return;            // channel not populated on this board
    g_dimCh[slot] = ch;
    zbDim[slot] = new ZigbeeDimmableLight(endpoint);
    zbDim[slot]->setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbDim[slot]->setPowerSource(ZB_POWER_SOURCE_MAINS);   // mains-powered router
  };
  auto newCct = [&](uint8_t slot, uint8_t endpoint, uint8_t cool, uint8_t warm) {
    if (!pairOn(cool, warm)) return;  // a CCT pair needs BOTH halves populated
    g_cctCool[slot] = cool;
    g_cctWarm[slot] = warm;
    zbCct[slot] = new ZigbeeColorDimmableLight(endpoint);
    zbCct[slot]->setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbCct[slot]->setPowerSource(ZB_POWER_SOURCE_MAINS);
    // Colour-temperature ONLY: advertising hue/saturation on a two-channel white
    // strip would let HA ask for colours the hardware cannot make.
    zbCct[slot]->setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP);
    zbCct[slot]->setLightColorTemperatureRange(MIRED_MIN, MIRED_MAX);
  };
  auto newRgb = [&](uint8_t endpoint, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    g_rgbR = r; g_rgbG = g; g_rgbB = b; g_rgbW = w;
    zbRgb = new ZigbeeColorDimmableLight(endpoint);
    zbRgb->setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbRgb->setPowerSource(ZB_POWER_SOURCE_MAINS);
    /* HUE/SAT ONLY — deliberately NOT X_Y. With XY advertised, Z2M converts every
     * colour to xy and sends moveToColor, so the device stores xy while the UI's
     * picker works in hue/saturation; the two models never meet and the picker
     * cannot be positioned. Advertising hue/sat alone makes Z2M send
     * moveToHueAndSaturation, so the device stores what the UI actually uses. */
    zbRgb->setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION);
  };

  switch (g_profile) {
    case PROFILE_4XDIM:
      newDim(0, 10, 0); newDim(1, 11, 1); newDim(2, 12, 2); newDim(3, 13, 3);
      break;
    case PROFILE_CCT_2DIM:
      // CCT pairs keep the LEGACY cw/ww pins, which ARE proven: the live
      // living-room board is already wired cw=IO6 ("W"), ww=IO5 ("B").
      newCct(0, 10, 0, 1);                 // CH1 cool (IO6/W) + CH2 warm (IO5/B)
      newDim(2, 12, 2); newDim(3, 13, 3);  // CH3, CH4 stay independent dimmers
      break;
    case PROFILE_2XCCT:
      newCct(0, 10, 0, 1);                 // CH1 cool (IO6/W) + CH2 warm (IO5/B)
      newCct(1, 12, 2, 3);                 // CH3 cool (IO4/R) + CH4 warm (IO2/G)
      break;
    case PROFILE_RGBW:
      // MEASURED: R=IO2(CH4) G=IO4(CH3) B=IO6(CH1) W=IO5(CH2).
      newRgb(10, 3, 2, 0, 1);
      break;
    case PROFILE_RGB_DIM:
      // Same R/G/B; the spare channel is the white one, IO5 (CH2).
      newRgb(10, 3, 2, 0, 0xFF);
      newDim(3, 13, 1);                    // dimmer on IO5 (the white channel)
      break;
    default:
      break;
  }
  void (*dimCb[4])(bool, uint8_t) = {onDim0, onDim1, onDim2, onDim3};
  for (uint8_t i = 0; i < 4; i++) if (zbDim[i]) zbDim[i]->onLightChange(dimCb[i]);
  if (zbCct[0]) zbCct[0]->onLightChangeTemp(onCct0);
  if (zbCct[1]) zbCct[1]->onLightChangeTemp(onCct1);
  if (zbRgb) {
    zbRgb->onLightChangeRgb(onRgbCb);
    zbRgb->onLightChangeHsv(onHsvCb);   // the one that actually fires in hs mode
  }

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
  zbMask.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  zbMask.addAnalogOutput();
  zbMask.setAnalogOutputDescription("Channel mask (bit0=CH1 .. bit3=CH4)");
  zbMask.setAnalogOutputMinMax(1, 15);
  zbMask.onAnalogOutputChange(onMaskWrite);
  zbAcCfg.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
  zbAcCfg.addBinaryOutput();
  zbAcCfg.setBinaryOutputDescription("AC enabled (LG split wired to the IR LED)");
  zbAcCfg.onBinaryOutputChange(onAcCfgWrite);

  // AC endpoints — only when a split is actually wired.
  if (g_acEnabled) {
    g_ir = new IRLgAc(IR_LED_PIN);
    g_ir->calibrate();
    g_ir->setModel(lg_ac_remote_model_t::AKB75215403);   // same remote as the HomeKit firmware
    g_ir->begin();

    zbAcMode.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbAcMode.addMultistateOutput();
    zbAcMode.setMultistateOutputStates(AC_MODE_COUNT);
    zbAcMode.setMultistateOutputDescription("AC mode");
    zbAcMode.onMultistateOutputChange(onAcModeWrite);

    zbAcTemp.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbAcTemp.addAnalogOutput();
    zbAcTemp.setAnalogOutputDescription("AC setpoint (C)");
    zbAcTemp.setAnalogOutputMinMax(AC_SETPOINT_MIN, AC_SETPOINT_MAX);
    zbAcTemp.onAnalogOutputChange(onAcTempWrite);

    zbAcFan.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbAcFan.setFanModeSequence(FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO);
    zbAcFan.onFanModeChange(onAcFanWrite);

    zbAcSwing.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbAcSwing.addBinaryOutput();
    zbAcSwing.setBinaryOutputDescription("AC vertical swing");
    zbAcSwing.onBinaryOutputChange(onAcSwingWrite);
  }

  // 7. Room sensor (only when fitted)
  if (g_haveAM2320) {
    zbRoom.setManufacturerAndModel(ZB_MANUFACTURER, ZB_MODEL);
    zbRoom.setMinMaxValue(-40, 80);
    zbRoom.setTolerance(0.5);
    zbRoom.addHumiditySensor(0, 100, 1.0, 0);
  }

  // 8. OTA client goes on whichever endpoint owns EP10 in this profile. It ships
  //    from v1 on purpose: flashing Zigbee removes the WiFi OTA path, and most of
  //    these boards are behind furniture.
  zbEp10 = zbDim[0] ? (ZigbeeEP *)zbDim[0]
         : zbCct[0] ? (ZigbeeEP *)zbCct[0]
         : (ZigbeeEP *)zbRgb;
  if (zbEp10) {
    zbEp10->addOTAClient(OTA_FW_RUNNING, OTA_FW_DOWNLOADED, OTA_HW_VERSION,
                         OTA_MANUFACTURER, OTA_IMAGE_TYPE);
  }

  for (uint8_t i = 0; i < 4; i++) if (zbDim[i]) Zigbee.addEndpoint(zbDim[i]);
  for (uint8_t i = 0; i < 2; i++) if (zbCct[i]) Zigbee.addEndpoint(zbCct[i]);
  if (zbRgb) Zigbee.addEndpoint(zbRgb);
  Zigbee.addEndpoint(&zbCfg);
  Zigbee.addEndpoint(&zbDiag);
  Zigbee.addEndpoint(&zbDie);
  Zigbee.addEndpoint(&zbMask);
  Zigbee.addEndpoint(&zbAcCfg);
  if (g_haveAM2320) Zigbee.addEndpoint(&zbRoom);
  if (g_acEnabled) {
    Zigbee.addEndpoint(&zbAcMode);
    Zigbee.addEndpoint(&zbAcTemp);
    Zigbee.addEndpoint(&zbAcFan);
    Zigbee.addEndpoint(&zbAcSwing);
  }

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
  g_maskEcho = true;
  zbMask.setAnalogOutput((float)g_chMask);
  g_acCfgEcho = true;
  zbAcCfg.setBinaryOutput(g_acEnabled);
  if (g_acEnabled) {
    // Publish the optimistic starting state WITHOUT sending IR: the split's real
    // state is unknown at boot and blasting a frame would change it uninvited.
    zbAcMode.setMultistateOutput(g_acMode);
    zbAcTemp.setAnalogOutput(g_acSetpoint);
    zbAcSwing.setBinaryOutput(g_acSwing);
    g_acDirty = false;
  }

  // 10. Restore each light from its persisted attributes so a power cut does not
  //     leave strips in a surprise state. This fires the change callbacks, which
  //     drive the PWM.
  for (uint8_t i = 0; i < 4; i++) if (zbDim[i]) zbDim[i]->restoreLight();
  for (uint8_t i = 0; i < 2; i++) if (zbCct[i]) zbCct[i]->restoreLight();
  if (zbRgb) zbRgb->restoreLight();
}

void loop() {
  const unsigned long now = millis();

  stepRamps();   // transition ramp for all four PWM channels

  // Apply a coalesced colour change once x and y have both landed.
  if (g_rgbPending && (long)(now - g_rgbAt) >= 0) applyRgbNow();

  // Coalesced IR frame for the AC (see acTouch()).
  if (g_acDirty && (long)(now - g_acSendAt) >= 0) acSendNow();

  // Deferred profile reboot — see onProfileWrite().
  if (g_rebootPending && (long)(now - g_rebootAt) >= 0) {
    Serial.println("Applying new profile now");
    delay(50);
    ESP.restart();
  }

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
