#include "Zigbee.h"
#include <Preferences.h>

/*
 * ESP32-C6 ZIGBEE IN-WALL SCENE / BUTTON MODULE
 * ---------------------------------------------
 * Alternative to the Philips Hue Wall Switch Module, but mains powered
 * (Hi-Link HLK-PM01 -> AMS1117-3.3 -> WT0132C6-S5), so it joins the
 * mesh as a ZIGBEE ROUTER instead of a sleepy end device.
 *
 * The WT0132C6-S5 is an ESP32-C6 module in the old ESP8266 ESP-12F land
 * pattern: side castellations, hand-solderable, drop-in for older boards.
 *
 * The two wall switches are wired to the module's low-voltage inputs and
 * reported to Zigbee2MQTT as BUTTON ACTIONS (single / double / long) — the
 * same model as a Hue dimmer or a Modomus scene switch, so HA maps each
 * channel to a Stateless Programmable Switch (Single / Double / Long press)
 * instead of a door/contact sensor.
 *
 * How the action is carried on Zigbee:
 *   Each channel is a Multistate Input endpoint (genMultistateInput,
 *   cluster 0x0012). On a gesture we write presentValue = 1/2/3 and report
 *   it, then reset to 0. The Z2M external converter maps
 *   presentValue -> action ('single'/'double'/'hold') per endpoint.
 *   (see /home/pi/zigbee2mqtt/data/external_converters/diy_esp32c6_2ch_input.js)
 *
 * Zigbee OTA: EP10 carries an OTA client; requestOTAUpdate() is started from
 * loop() only after the mesh is up, so field units can be updated wirelessly.
 *
 * Arduino IDE settings (arduino-esp32 core 3.x):
 *   Board:          ESP32C6 Dev Module  (or XIAO_ESP32C6 for the prototype)
 *   Zigbee Mode:    Zigbee ZCZR (coordinator/router)
 *   Partition:      Zigbee ZCZR 4MB with spiffs
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
// No status LED: on the as-built board the LED net lands on GPIO17 (UART0 RX),
// which we keep for serial debug, so the LED is dropped (device lives in a wall).
#endif

/* --- GESTURE TIMING (per input) --- */
#define DEBOUNCE_MS      25    // Raw edge must be stable this long
#define MULTI_WINDOW_MS  300   // Max gap between clicks to count as a multi-click
#define LONG_PRESS_MS    600   // Hold at least this long = 'long'
#define ACTION_CLEAR_MS  150   // After firing, reset multistate 0 so repeats re-report

/* --- Gesture codes reported on genMultistateInput presentValue --- */
#define ACT_IDLE   0
#define ACT_SINGLE 1
#define ACT_DOUBLE 2
#define ACT_LONG   3

/* --- OTA (Zigbee firmware update over the mesh) --- */
// Bump OTA_FW_RUNNING on every release so Z2M offers a newer image to units
// already in the field. Version is 0xMMmmpprr — this build is v1.0.0.0.
// MANUFACTURER + IMAGE_TYPE must match the Z2M OTA index entry and the .ota header.
#define OTA_FW_RUNNING     0x01000000
#define OTA_FW_DOWNLOADED  0x01000001
#define OTA_HW_VERSION     0x0101
#define OTA_MANUFACTURER   0x1001
#define OTA_IMAGE_TYPE     0x1011

/* --- ZIGBEE ENDPOINTS: two Multistate Input "buttons" --- */
ZigbeeMultistate zbBtn1 = ZigbeeMultistate(10);
ZigbeeMultistate zbBtn2 = ZigbeeMultistate(11);

/* --- PER-INPUT GESTURE STATE --- */
struct Button {
  uint8_t pin;
  ZigbeeMultistate *ep;
  bool rawLast;                 // last raw level read
  bool pressed;                 // debounced pressed state (true = shorted to GND)
  unsigned long lastEdge;       // millis() of last raw transition
  unsigned long pressStart;     // millis() when the current press began
  bool longFired;               // long already emitted for this hold
  uint8_t clickCount;           // clicks accumulated in the current burst
  unsigned long lastRelease;    // millis() of last release
  bool awaiting;                // waiting to see if another click arrives
  unsigned long clearAt;        // millis() to reset presentValue to 0 (0 = idle)
};

Button buttons[2] = {
  { SW1_PIN, &zbBtn1, HIGH, false, 0, 0, false, 0, 0, false, 0 },
  { SW2_PIN, &zbBtn2, HIGH, false, 0, 0, false, 0, 0, false, 0 },
};

Preferences prefs;

// Factory-reset button (IO9 / BOOT) tracking
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

#ifdef HAS_RF_SWITCH
bool isExternalAntenna = false; // Tracks current antenna mode
int clickCount = 0;             // Triple-click toggles internal/external antenna
unsigned long lastClickTime = 0;
#endif

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

/* --- EMIT ONE GESTURE AS A MULTISTATE REPORT --- */
void fireAction(Button &b, uint8_t code) {
  const char *name = code == ACT_SINGLE ? "single" : code == ACT_DOUBLE ? "double" : "long";
  Serial.printf("Button GPIO%d -> %s\n", b.pin, name);
  if (Zigbee.connected()) {
    b.ep->setMultistateInput(code);
    b.ep->reportMultistateInput();
    b.clearAt = millis() + ACTION_CLEAR_MS;  // reset to 0 shortly after
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef HAS_STATUS_LED
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW); // LED ON = Booting
#endif

  // 1. Configure the two switch inputs (internal pull-up; the final board adds
  //    an external 10k + RC on top for noise immunity on in-wall runs).
  for (auto &b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
    b.rawLast = digitalRead(b.pin);
    b.pressed = (b.rawLast == LOW);
  }

#ifdef HAS_RF_SWITCH
  // 2. Load antenna preference and apply it BEFORE starting Zigbee
  prefs.begin("zigbee-cfg", false);
  isExternalAntenna = prefs.getBool("ext_ant", false); // Default to internal
  prefs.end();
  applyAntennaConfig(isExternalAntenna);
#endif

  // 3. Zigbee identity — both endpoints carry the same Basic cluster info
  zbBtn1.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");
  zbBtn2.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");

  // 4. Each channel is a Multistate Input with 4 states (idle/single/double/long)
  zbBtn1.addMultistateInput();
  zbBtn1.setMultistateInputStates(4);
  zbBtn1.setMultistateInputDescription("Button 1");
  zbBtn2.addMultistateInput();
  zbBtn2.setMultistateInputStates(4);
  zbBtn2.setMultistateInputDescription("Button 2");

  // OTA client on EP10 — lets Z2M push firmware over Zigbee, so once deployed
  // the device updates wirelessly (no wired flashing / bodge header).
  zbBtn1.addOTAClient(OTA_FW_RUNNING, OTA_FW_DOWNLOADED, OTA_HW_VERSION,
                      OTA_MANUFACTURER, OTA_IMAGE_TYPE);

  Zigbee.addEndpoint(&zbBtn1);
  Zigbee.addEndpoint(&zbBtn2);

  // Router mode: the module is mains powered, so it strengthens the mesh —
  // the whole point of replacing the battery-powered Hue wall module.
  Zigbee.begin(ZIGBEE_ROUTER);
  // NOTE: requestOTAUpdate() is deliberately NOT called here — the network is
  // not up yet. It is kicked off once from loop() after Zigbee.connected().
}

/* --- Run the gesture state-machine for one input --- */
void serviceButton(Button &b, unsigned long now) {
  // Debounce the raw level into b.pressed, and detect press/release edges.
  bool raw = digitalRead(b.pin);
  if (raw != b.rawLast) {
    b.lastEdge = now;
    b.rawLast = raw;
  }
  if ((now - b.lastEdge) > DEBOUNCE_MS) {
    bool nowPressed = (raw == LOW);
    if (nowPressed && !b.pressed) {
      // ---- PRESS edge ----
      b.pressed = true;
      b.pressStart = now;
      b.longFired = false;
    } else if (!nowPressed && b.pressed) {
      // ---- RELEASE edge ----
      b.pressed = false;
      if (!b.longFired) {
        b.clickCount++;
        if (b.clickCount >= 2) {
          fireAction(b, ACT_DOUBLE);   // second click -> double immediately
          b.clickCount = 0;
          b.awaiting = false;
        } else {
          b.lastRelease = now;         // wait to see if a second click comes
          b.awaiting = true;
        }
      }
    }
  }

  // Hold detection: fire 'long' once the press passes the threshold.
  if (b.pressed && !b.longFired && (now - b.pressStart) >= LONG_PRESS_MS) {
    fireAction(b, ACT_LONG);
    b.longFired = true;
    b.clickCount = 0;
    b.awaiting = false;
  }

  // Single-click resolves once the multi-click window closes with one click.
  if (b.awaiting && (now - b.lastRelease) > MULTI_WINDOW_MS) {
    fireAction(b, ACT_SINGLE);
    b.clickCount = 0;
    b.awaiting = false;
  }

  // Reset presentValue to idle so a repeat of the same gesture re-reports.
  if (b.clearAt && now >= b.clearAt) {
    if (Zigbee.connected()) {
      b.ep->setMultistateInput(ACT_IDLE);
      b.ep->reportMultistateInput();
    }
    b.clearAt = 0;
  }
}

void loop() {
  unsigned long now = millis();

  // --- 0. START OTA POLLING once, after the network is actually up ---
  static bool otaRequested = false;
  if (!otaRequested && Zigbee.connected()) {
    otaRequested = true;
    zbBtn1.requestOTAUpdate();   // first query ~1 min later, then hourly
    Serial.println("OTA update polling started");
  }

  // --- 1. GESTURE INPUTS ---
  for (auto &b : buttons) serviceButton(b, now);

  // --- 2. FACTORY-RESET BUTTON (IO9 / BOOT) ---
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
#ifdef HAS_RF_SWITCH
    if (now - lastClickTime > 600) {
      clickCount = 1; // Reset click count if too much time passed
    } else {
      clickCount++;
    }
    lastClickTime = now;
#endif
    buttonPressed = true;
    buttonPressTime = now;
  }

  if (lastButtonState == LOW && currentButtonState == HIGH) {
    buttonPressed = false;

#ifdef HAS_RF_SWITCH
    // Triple Click: toggle internal/external antenna (XIAO prototype only)
    if (clickCount == 3) {
      isExternalAntenna = !isExternalAntenna;

      prefs.begin("zigbee-cfg", false);
      prefs.putBool("ext_ant", isExternalAntenna);
      prefs.end();

      Serial.println(isExternalAntenna ? "SWITCHED TO EXTERNAL ANTENNA" : "SWITCHED TO INTERNAL ANTENNA");
      for (int i = 0; i < (isExternalAntenna ? 4 : 2); i++) {
        digitalWrite(STATUS_LED, LOW);  delay(250);
        digitalWrite(STATUS_LED, HIGH); delay(250);
      }
      ESP.restart(); // Reboot to initialize Zigbee on the new antenna safely
    }
#endif
  }

  // Long hold on the BOOT button = factory reset
  if (buttonPressed && (now - buttonPressTime > 5000)) {
    triggerFactoryReset();
    buttonPressed = false;
  }

  lastButtonState = currentButtonState;

  // --- 3. CONNECTION STATUS LED (only if the board has one) ---
#ifdef HAS_STATUS_LED
  static unsigned long lastBlinkTime = 0;
  static bool blinkState = false;
  static bool wasConnected = false;
  static unsigned long connectedTime = 0;
  if (!Zigbee.connected()) {
    if (now - lastBlinkTime > 500) {
      lastBlinkTime = now;
      blinkState = !blinkState;
      digitalWrite(STATUS_LED, blinkState ? LOW : HIGH); // Blink while searching
    }
    wasConnected = false;
  } else {
    if (!wasConnected) {
      wasConnected = true;
      connectedTime = now;
      digitalWrite(STATUS_LED, LOW); // Solid ON when connected
    } else if (now - connectedTime > 3000) {
      digitalWrite(STATUS_LED, HIGH); // Off after 3s — it lives inside a wall box
    }
  }
#endif

  delay(5);
}
