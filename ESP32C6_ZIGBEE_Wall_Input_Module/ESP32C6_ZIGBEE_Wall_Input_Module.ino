#include "Zigbee.h"
#include <Preferences.h>

/*
 * ESP32-C6 ZIGBEE IN-WALL INPUT MODULE
 * ------------------------------------
 * Alternative to the Philips Hue Wall Switch Module, but mains powered
 * (Hi-Link HLK-PM01 -> AMS1117-3.3 -> WT0132C6-S5), so it joins the
 * mesh as a ZIGBEE ROUTER instead of a sleepy end device.
 *
 * The WT0132C6-S5 is an ESP32-C6 module in the old ESP8266 ESP-12F land
 * pattern: side castellations, hand-solderable, drop-in for older boards.
 *
 * The lamp circuit is bridged permanently live (smart bulbs stay powered);
 * the existing wall switches are rewired to the module's two low-voltage
 * inputs and exposed to Zigbee2MQTT as two contact sensors (IAS zone,
 * endpoints 10 and 11).
 *
 * Identifies cleanly via external_converter on Z2M side (see
 * /home/pi/zigbee2mqtt/data/external_converters/diy_esp32c6_2ch_input.js)
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
#define HAS_RF_SWITCH      // XIAO has the internal/external antenna RF switch
#endif

#ifdef BOARD_WT0132C6_S5
/* --- WT0132C6-S5 (final board, ESP-12F land pattern) --- */
#define SW1_PIN 4          // IO4: wall switch to GND (ext. 10k pull-up + RC)
#define SW2_PIN 5          // IO5: wall switch to GND (ext. 10k pull-up + RC)
#define BUTTON_PIN 9       // IO9, bottom castellation (pairing / factory reset)
#define STATUS_LED 18      // IO18, active LOW — GPIO15 is not exposed on this module
#endif

#define DEBOUNCE_MS 40     // Long in-wall runs pick up noise; debounce hard

/* --- ZIGBEE ENDPOINTS --- */
ZigbeeContactSwitch zbInput1 = ZigbeeContactSwitch(10);
ZigbeeContactSwitch zbInput2 = ZigbeeContactSwitch(11);

/* --- STATE VARIABLES --- */
struct SwitchInput {
  uint8_t pin;
  ZigbeeContactSwitch *endpoint;
  bool stableState;        // Debounced level (HIGH = open, LOW = closed)
  bool lastReading;        // Raw level seen on previous loop pass
  unsigned long lastEdge;  // millis() of the last raw transition
};

SwitchInput inputs[2] = {
  { SW1_PIN, &zbInput1, HIGH, HIGH, 0 },
  { SW2_PIN, &zbInput2, HIGH, HIGH, 0 },
};

Preferences prefs;

// Button tracking (factory reset hold + triple-click on XIAO)
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

#ifdef HAS_RF_SWITCH
bool isExternalAntenna = false; // Tracks current antenna mode
int clickCount = 0;             // Triple-click toggles internal/external antenna
unsigned long lastClickTime = 0;
#endif

// Push debounced states to Zigbee once the network is up
bool initialStateReported = false;

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
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED, LOW);  delay(150);
    digitalWrite(STATUS_LED, HIGH); delay(150);
  }
  Zigbee.factoryReset();
}

/* --- PUSH ONE INPUT STATE TO ZIGBEE --- */
void reportInput(SwitchInput &in) {
  // Switch shorts the input to GND: LOW = contact closed
  if (in.stableState == LOW) {
    in.endpoint->setClosed();
  } else {
    in.endpoint->setOpen();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW); // LED ON = Booting

  // 1. Configure switch inputs (internal pull-up; the final board adds an
  //    external 10k + RC on top for noise immunity on in-wall runs)
  for (auto &in : inputs) {
    pinMode(in.pin, INPUT_PULLUP);
    in.stableState = digitalRead(in.pin);
    in.lastReading = in.stableState;
  }

#ifdef HAS_RF_SWITCH
  // 2. Load antenna preference and apply it BEFORE starting Zigbee
  prefs.begin("zigbee-cfg", false);
  isExternalAntenna = prefs.getBool("ext_ant", false); // Default to internal
  prefs.end();
  applyAntennaConfig(isExternalAntenna);
#endif

  // 3. Zigbee identity — both endpoints carry the same Basic cluster info
  zbInput1.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");
  zbInput2.setManufacturerAndModel("DIY", "ESP32C6-2CH-INPUT");

  Zigbee.addEndpoint(&zbInput1);
  Zigbee.addEndpoint(&zbInput2);

  // Router mode: the module is mains powered, so it strengthens the mesh —
  // the whole point of replacing the battery-powered Hue wall module.
  Zigbee.begin(ZIGBEE_ROUTER);
}

void loop() {
  unsigned long currentMillis = millis();

  // --- 1. DEBOUNCED SWITCH INPUTS ---
  for (auto &in : inputs) {
    bool reading = digitalRead(in.pin);
    if (reading != in.lastReading) {
      in.lastEdge = currentMillis;   // Raw edge: restart the debounce window
      in.lastReading = reading;
    }
    if ((currentMillis - in.lastEdge) > DEBOUNCE_MS && reading != in.stableState) {
      in.stableState = reading;
      Serial.printf("Input GPIO%d -> %s\n", in.pin, reading == LOW ? "CLOSED" : "OPEN");
      reportInput(in);
    }
  }

  // --- 2. MULTI-FUNCTION BUTTON LOGIC ---
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Detect Button Press (HIGH to LOW transition)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
#ifdef HAS_RF_SWITCH
    if (currentMillis - lastClickTime > 600) {
      clickCount = 1; // Reset click count if too much time passed
    } else {
      clickCount++;
    }
    lastClickTime = currentMillis;
#endif
    buttonPressed = true;
    buttonPressTime = currentMillis;
  }

  // Detect Button Release (LOW to HIGH transition)
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
      // Visual feedback: 2 slow blinks = internal, 4 = external
      for (int i = 0; i < (isExternalAntenna ? 4 : 2); i++) {
        digitalWrite(STATUS_LED, LOW);  delay(250);
        digitalWrite(STATUS_LED, HIGH); delay(250);
      }
      ESP.restart(); // Reboot to initialize Zigbee on the new antenna safely
    }
#endif
  }

  // Check for Long Hold (Reset)
  if (buttonPressed && (currentMillis - buttonPressTime > 5000)) {
    triggerFactoryReset();
    buttonPressed = false; // Prevent re-triggering
  }

  lastButtonState = currentButtonState;

  // --- 3. STATUS LED LOGIC ---
  static unsigned long lastBlinkTime = 0;
  static bool blinkState = false;
  static bool wasConnected = false;
  static unsigned long connectedTime = 0;

  if (!Zigbee.connected()) {
    if (currentMillis - lastBlinkTime > 500) {
      lastBlinkTime = currentMillis;
      blinkState = !blinkState;
      digitalWrite(STATUS_LED, blinkState ? LOW : HIGH); // Blink while searching
    }
    wasConnected = false;
    initialStateReported = false;
  } else {
    if (!wasConnected) {
      wasConnected = true;
      connectedTime = currentMillis;
      digitalWrite(STATUS_LED, LOW); // Solid ON when connected
    } else if (currentMillis - connectedTime > 3000) {
      digitalWrite(STATUS_LED, HIGH); // Turn off after 3s — it lives inside a wall box anyway
    }

    // Sync both contact states once after (re)joining so Z2M never shows stale data
    if (!initialStateReported && (currentMillis - connectedTime > 2000)) {
      initialStateReported = true;
      for (auto &in : inputs) reportInput(in);
    }
  }

  delay(10);
}
