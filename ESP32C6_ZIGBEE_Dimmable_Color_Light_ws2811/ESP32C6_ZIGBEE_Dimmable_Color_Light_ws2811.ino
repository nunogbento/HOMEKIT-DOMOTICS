#include "Zigbee.h"
#include <FastLED.h>
#include <Preferences.h>

/* --- XIAO ESP32C6 CONFIGURATION --- */
#define NUM_LEDS 100       
#define DATA_PIN 21        // Replaced 'D3' with the raw GPIO 21
#define MAX_AMPS 4500      
#define BUTTON_PIN 9       // BOOT button is GPIO 9
#define STATUS_LED 15      // Replaced 'LED_BUILTIN' with raw GPIO 15

CRGB leds[NUM_LEDS];

/* --- STATE VARIABLES --- */
uint8_t targetBrightness = 0;
uint8_t currentBrightness = 0;
CRGB targetColor = CRGB::White; 
Preferences prefs;
ZigbeeColorDimmableLight zbLight = ZigbeeColorDimmableLight(11);
bool isExternalAntenna = false; // Tracks current antenna mode

// Timer variables
unsigned long bootTime = 0;
bool counterCleared = false;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;

// Triple-click tracking
int clickCount = 0;
unsigned long lastClickTime = 0;
bool lastButtonState = HIGH;

/* --- ANTENNA SWITCH FUNCTION --- */
void applyAntennaConfig(bool useExternal) {
  pinMode(3, OUTPUT);
  pinMode(14, OUTPUT);
  if (useExternal) {
    digitalWrite(3, LOW);  // Turn ON RF switch power
    digitalWrite(14, HIGH); // Route to U.FL connector
    Serial.println("RF Mode: EXTERNAL (U.FL)");
  } else {
    digitalWrite(3, HIGH); // Turn OFF RF switch power 
    digitalWrite(14, LOW); // Route to Internal Ceramic
    Serial.println("RF Mode: INTERNAL (Ceramic)");
  }
}

/* --- RESET HELPER FUNCTION --- */
void triggerFactoryReset() {
  Serial.println("FACTORY RESET TRIGGERED!");
  for(int i=0; i<3; i++) {
      fill_solid(leds, NUM_LEDS, CRGB::Red); FastLED.setBrightness(255); FastLED.show(); delay(300);
      fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); delay(300);
  }
  prefs.begin("zigbee-cfg", false);
  prefs.putInt("boot_count", 0);
  prefs.end();
  Zigbee.factoryReset(); 
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW); // LED ON = Booting

  // 1. Initialize COB Strip
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(12, MAX_AMPS);
  FastLED.setBrightness(255); 

  // 2. LOAD PREFERENCES (Boot Count & Antenna State)
  prefs.begin("zigbee-cfg", false);
  int bootCount = prefs.getInt("boot_count", 0);
  isExternalAntenna = prefs.getBool("ext_ant", false); // Default to internal (false)
  
  bootCount++;
  prefs.putInt("boot_count", bootCount);
  prefs.end();

  // Apply the loaded antenna setting BEFORE starting Zigbee
  applyAntennaConfig(isExternalAntenna);

  // Disabled during debugging — was wiping pairing state on rapid resets.
  // if (bootCount >= 5) triggerFactoryReset();

  // Identifies cleanly via external_converter on Z2M side (see
  // /home/pi/zigbee2mqtt/data/external_converters/diy_esp32c6_ws2811.js)
  zbLight.setManufacturerAndModel("DIY", "ESP32C6-WS2811");

  zbLight.onLightChangeRgb([](bool state, uint8_t r, uint8_t g, uint8_t b, uint8_t level) {
    if (!state) {
      targetBrightness = 0; 
    } else {
      targetColor = CRGB(r, g, b);
      targetBrightness = level;
    }
  });

  Zigbee.addEndpoint(&zbLight);
  // Router mode: Hue bridge expects mains-powered lights to act as routers on the mesh.
  Zigbee.begin(ZIGBEE_ROUTER);
  bootTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  bool needsUpdate = false;

  // --- 1. BOOT COUNTER CLEAR ---
  if (!counterCleared && (currentMillis - bootTime > 5000)) {
    prefs.begin("zigbee-cfg", false);
    prefs.putInt("boot_count", 0);
    prefs.end();
    counterCleared = true;
  }

  // --- 2. MULTI-FUNCTION BUTTON LOGIC ---
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detect Button Press (HIGH to LOW transition)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (currentMillis - lastClickTime > 600) {
      clickCount = 1; // Reset click count if too much time passed
    } else {
      clickCount++;
    }
    lastClickTime = currentMillis;
    buttonPressed = true;
    buttonPressTime = currentMillis;
  }

  // Detect Button Release (LOW to HIGH transition)
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    buttonPressed = false;
    
    // Check for Triple Click
    if (clickCount == 3) {
      isExternalAntenna = !isExternalAntenna; // Toggle state
      
      // Save new setting to NVS
      prefs.begin("zigbee-cfg", false);
      prefs.putBool("ext_ant", isExternalAntenna);
      prefs.putInt("boot_count", 0); // Prevent boot loop trigger on restart
      prefs.end();

      // Visual Feedback
      Serial.println(isExternalAntenna ? "SWITCHED TO EXTERNAL ANTENNA" : "SWITCHED TO INTERNAL ANTENNA");
      fill_solid(leds, NUM_LEDS, isExternalAntenna ? CRGB::Blue : CRGB::Green);
      FastLED.setBrightness(255);
      FastLED.show();
      
      delay(1500); // Let the user see the color confirmation
      ESP.restart(); // Reboot to initialize Zigbee on the new antenna safely
    }
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
  } else {
    if (!wasConnected) {
      wasConnected = true;
      connectedTime = currentMillis;
      digitalWrite(STATUS_LED, LOW); // Solid ON when connected
    } else if (currentMillis - connectedTime > 3000) {
      digitalWrite(STATUS_LED, HIGH); // Turn off after 3s so it doesn't glow in the dark
    }
  }

  // --- 4. SMOOTH COB FADE ENGINE ---
  if (currentBrightness != targetBrightness) {
    if (currentBrightness < targetBrightness) currentBrightness++;
    else currentBrightness--;
    
    CRGB fadeColor = targetColor;
    fadeColor.nscale8(currentBrightness);
    fill_solid(leds, NUM_LEDS, fadeColor);
    needsUpdate = true;
  }

  if (needsUpdate) {
    FastLED.show();
    if (currentBrightness != targetBrightness) delay(3); 
  }
}