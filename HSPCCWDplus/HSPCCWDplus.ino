
#include "HomeSpan.h"
#include <Wire.h>
#include <LittleFS.h>

// PCCWD Bus components (hardcoded)
#include "PCCWDController.h"
#include "OF8WdOutput.h"
#include "IN10WdStatelessProgrammableSwitch.h"
#include "DEV_Identify.h"

// MCP23017 GPIO Expander components (web configurable)
#include "MCP23017Handler.h"
#include "ConfigManager.h"
#include "PinAccessoryFactory.h"
#include "WebConfigServer.h"

// PWM Dimmable LED (hardcoded on GPIO2)
#include "DimmableLED.h"

// Pin assignments for ESP32-C6 (WT0132C6-S5)
#define STATUS_LED_PIN    15
#define CONTROL_BTN_PIN   0
#define PCCWD_RX_PIN      6
#define PCCWD_TX_PIN      7
#define I2C_SDA_PIN       10  // ESP12F IO4 → ESP32-C6 GPIO10
#define I2C_SCL_PIN       3   // ESP12F IO5 → ESP32-C6 GPIO3
#define LED_PWM_PIN       2   // MOSFET driver for 12V LED dimming

// MCP23017 I2C address (0x20 default, A0-A2 all LOW)
#define MCP23017_ADDR     0x20

// PCCWD Bus accessories (conditionally created based on config)
IN10wDStatelessProgrammableSwitch* switches[NUM_IN10WD] = {nullptr};
OF8WdOutput* outputs[NUM_OF8WD] = {nullptr};
PCCWDController pccwdController(Serial1);

// PCCWD bus addresses (loaded from config)
uint8_t of8wdBaseAddress = DEFAULT_OF8WD_ADDRESS;
uint8_t in10wdBaseAddress = DEFAULT_IN10WD_ADDRESS;

// MCP23017 GPIO Expander (web configurable)
MCP23017Handler mcpHandler(MCP23017_ADDR);
ConfigManager configManager;
PinAccessoryFactory pinFactory;
WebConfigServer webServer;

// Sensor polling interval
const unsigned long SENSOR_POLL_INTERVAL = 100;  // 100ms
unsigned long lastSensorPoll = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial
  Serial.println("\n\n=== PCCWD Bridge Starting ===");

  // Initialize PCCWD serial
  Serial1.begin(14400, SERIAL_8N1, PCCWD_RX_PIN, PCCWD_TX_PIN);
  Serial.println("PCCWD Serial initialized");

  // Initialize I2C for MCP23017
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C initialized");

  // Initialize MCP23017
  if (mcpHandler.begin()) {
    Serial.printf("MCP23017 found at address 0x%02X\n", MCP23017_ADDR);
  } else {
    Serial.println("WARNING: MCP23017 not found!");
  }

  // Initialize LittleFS and load configuration
  if (!configManager.begin()) {
    Serial.println("WARNING: Config manager failed to initialize");
  } else {
    Serial.printf("Config loaded: %d accessories configured\n", configManager.countAccessories());
    // Load PCCWD bus addresses from config
    of8wdBaseAddress = configManager.getOF8WdAddress();
    in10wdBaseAddress = configManager.getIN10WdAddress();
    Serial.printf("PCCWD addresses: OF8Wd=%d, IN10Wd=%d\n", of8wdBaseAddress, in10wdBaseAddress);
  }

  // Configure HomeSpan
  homeSpan.setStatusPin(STATUS_LED_PIN);
  homeSpan.setControlPin(CONTROL_BTN_PIN);
  homeSpan.setApSSID("PCCWDSETUP");
  homeSpan.setApPassword("");
  homeSpan.enableAutoStartAP();

  homeSpan.begin(Category::Bridges, "PCCWD", "PCCWD", "PCCWD Bridge");

  // Create bridge accessory
  new SpanAccessory();
  new DEV_Identify("PCCWD", "MORDOMUS", "000000", "PCCWD", "0.2", 3);

  // ============================================
  // PCCWD Bus Accessories (conditionally created)
  // ============================================

  // Create OF8Wd outputs (lightbulbs) - only if enabled in config
  int of8wdCount = 0;
  for (int i = 0; i < NUM_OF8WD; i++) {
    if (configManager.isOF8WdEnabled(i)) {
      const HardcodedDeviceConfig& cfg = configManager.getOF8WdConfig(i);
      new SpanAccessory();
      new DEV_Identify(cfg.name.c_str(), "MORDOMUS", "000000", "OF8Wd", "0.1", 0);
      outputs[i] = new OF8WdOutput(of8wdBaseAddress + i);
      outputs[i]->setupdate_calback([](byte address, bool newPowerState) {
        pccwdController.setOnSate(address, newPowerState);
      });
      of8wdCount++;
    }
  }
  Serial.printf("OF8Wd outputs created: %d of %d enabled\n", of8wdCount, NUM_OF8WD);

  // Create IN10Wd inputs (stateless programmable switches) - only if enabled in config
  int in10wdCount = 0;
  for (int i = 0; i < NUM_IN10WD; i++) {
    if (configManager.isIN10WdEnabled(i)) {
      const HardcodedDeviceConfig& cfg = configManager.getIN10WdConfig(i);
      new SpanAccessory();
      new DEV_Identify(cfg.name.c_str(), "MORDOMUS", "000000", "IN10wd", "0.1", 0);
      switches[i] = new IN10wDStatelessProgrammableSwitch(in10wdBaseAddress + i);
      in10wdCount++;
    }
  }
  Serial.printf("IN10Wd switches created: %d of %d enabled\n", in10wdCount, NUM_IN10WD);

  // Set up PCCWD switch event callback (only triggers for enabled switches)
  pccwdController.setProgrammableSwitchEvent_callback([](uint8_t id, PressType pressType) {
    uint8_t index = id - in10wdBaseAddress;
    if (index < NUM_IN10WD && switches[index] != nullptr) {
      LOG_D("Switch pressed at address: %u type: %u", index, (int)pressType);
      switches[index]->Pressed((int)pressType);
    }
  });

  // ============================================
  // PWM Dimmable LED (conditionally created)
  // ============================================

  if (configManager.isPwmLedEnabled()) {
    const HardcodedDeviceConfig& ledCfg = configManager.getPwmLedConfig();
    new SpanAccessory();
    new DEV_Identify(ledCfg.name.c_str(), "MORDOMUS", "000000", "PWM LED", "0.1", 0);
    new DimmableLED(LED_PWM_PIN);
    Serial.println("PWM Dimmable LED created on GPIO2");
  } else {
    Serial.println("PWM Dimmable LED disabled in config");
  }

  // ============================================
  // MCP23017 GPIO Expander Accessories (configurable)
  // ============================================

  // Create accessories from configuration
  if (mcpHandler.isConnected()) {
    pinFactory.createAccessories(configManager, mcpHandler);
    Serial.println("MCP23017 accessories created");
  }

  // ============================================
  // Start Web Server
  // ============================================

  webServer.begin(configManager, mcpHandler);
  teeSerial.setWebServer(&webServer);
  Serial.println("Web configuration server started");

  Serial.println("=== Setup Complete ===\n");

} // end of setup()

//////////////////////////////////////

void loop() {
  // HomeSpan polling
  homeSpan.poll();

  // PCCWD bus polling
  pccwdController.loop();

  // Web server polling (for WebSocket)
  webServer.loop();

  // MCP23017 sensor polling (at reduced rate)
  unsigned long now = millis();
  if (now - lastSensorPoll >= SENSOR_POLL_INTERVAL) {
    lastSensorPoll = now;

    if (mcpHandler.isConnected()) {
      pinFactory.updateSensorStates();
    }
  }

} // end of loop()
