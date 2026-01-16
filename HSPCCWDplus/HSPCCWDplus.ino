
#include "HomeSpan.h"
#include <Wire.h>
#include <LittleFS.h>

// PCCWD Bus components (hardcoded)
#include "PCCWDController.h"
#include "OF8WdOutput.h"
#include "IN10WdStatelessProgrammableSwitch.h"
#include "OF8WdAccessoryInfo.h"
#include "AccessoryInfo.h"

// MCP23017 GPIO Expander components (web configurable)
#include "MCP23017Handler.h"
#include "ConfigManager.h"
#include "PinAccessoryFactory.h"

// Web server with WebSocket for HomeSpan CLI
#include "WebConfigServer.h"

// PWM Dimmable LED (hardcoded on GPIO2)
#include "DimmableLED.h"
#include "PWMLEDAccessoryInfo.h"

// Pin assignments for ESP32-C6 (WT0132C6-S5)
#define STATUS_LED_PIN    8
#define CONTROL_BTN_PIN   9
#define SERIAL_RX_PIN     17   // USB/Debug Serial RX
#define SERIAL_TX_PIN     16   // USB/Debug Serial TX
#define PCCWD_RX_PIN      0   // PCCWD Bus RX (Serial1)
#define PCCWD_TX_PIN      1   // PCCWD Bus TX (Serial1)
#define I2C_SDA_PIN       10  // ESP12F IO4 → ESP32-C6 GPIO10
#define I2C_SCL_PIN       3   // ESP12F IO5 → ESP32-C6 GPIO3
#define LED_PWM_PIN       2   // MOSFET driver for 12V LED dimming

// MCP23017 I2C address (0x20 default, A0-A2 all LOW)
#define MCP23017_ADDR     0x20

// PCCWD Bus accessories (conditionally created based on config)
IN10wDStatelessProgrammableSwitch* switches[NUM_IN10WD] = {nullptr};
OF8WdOutput* outputs[NUM_OF8WD] = {nullptr};
OF8WdAccessoryInfo* of8wdAccessoryInfo[NUM_OF8WD] = {nullptr};
PCCWDController pccwdController(Serial1);

// PCCWD bus addresses (loaded from config)
uint8_t of8wdBaseAddress = DEFAULT_OF8WD_ADDRESS;
uint8_t in10wdBaseAddress = DEFAULT_IN10WD_ADDRESS;

// MCP23017 GPIO Expander (web configurable)
MCP23017Handler mcpHandler(MCP23017_ADDR);
ConfigManager configManager;
PinAccessoryFactory pinFactory;
WebConfigServer* webServer = nullptr;
PWMLEDAccessoryInfo* pwmLedAccessoryInfo = nullptr;

// Sensor polling interval
const unsigned long SENSOR_POLL_INTERVAL = 100;  // 100ms
unsigned long lastSensorPoll = 0;

// Fixed accessory ID slots (IDs must remain stable across config changes)
// Bridge: 1, IN10Wd: 2-11, OF8Wd: 12-19, PWM LED: 20, MCP pins: 21-36, Security: 37
#define AID_BRIDGE      1
#define AID_IN10WD_BASE 2    // 2-11 (10 switches)
#define AID_OF8WD_BASE  12   // 12-19 (8 outputs)
#define AID_PWM_LED     20
#define AID_MCP_BASE    21   // 21-36 (16 pins)
#define AID_SECURITY    37

void setupWeb(){
  webServer = new WebConfigServer();
  webServer->begin(configManager, mcpHandler);
  LOG_D("Web config server started at http://%s/\n", WiFi.localIP().toString().c_str());
}


void setup() {
  // Use Serial0 (UART0) on GPIO6/7 for external USB-serial adapter
  Serial.begin(115200, SERIAL_8N1, SERIAL_RX_PIN, SERIAL_TX_PIN);
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
  homeSpan.setLogLevel(1);
  homeSpan.setApPassword("");
  homeSpan.enableAutoStartAP();
  homeSpan.setPortNum(8080);  // Move HomeSpan off port 80 for web server
  homeSpan.setWifiCallback(setupWeb);     // need to start Web Server after WiFi is established

  homeSpan.begin(Category::Bridges, "PCCWD", "PCCWD", "PCCWD Bridge");

  // Create bridge accessory (fixed ID 1)
  char serialNum[16];
  snprintf(serialNum, sizeof(serialNum), "AID-%03d", AID_BRIDGE);
  new SpanAccessory(AID_BRIDGE);
  new AccessoryInfo("PCCWD", "MORDOMUS", serialNum, "PCCWD", "0.2");

  // ============================================
  // PCCWD Bus Accessories (conditionally created)
  // ============================================

  // Create OF8Wd outputs (lightbulbs) - only if enabled in config
  // Fixed IDs: 12-19 (AID_OF8WD_BASE + index)
  int of8wdCount = 0;
  for (int i = 0; i < NUM_OF8WD; i++) {
    if (configManager.isOF8WdEnabled(i)) {
      const HardcodedDeviceConfig& cfg = configManager.getOF8WdConfig(i);
      uint32_t aid = AID_OF8WD_BASE + i;
      uint8_t address = of8wdBaseAddress + i;
      snprintf(serialNum, sizeof(serialNum), "AID-%03d", aid);
      new SpanAccessory(aid);
      of8wdAccessoryInfo[i] = new OF8WdAccessoryInfo(cfg.name.c_str(), "MORDOMUS", serialNum, "OF8Wd", "0.1", address, &pccwdController, 3);
      outputs[i] = new OF8WdOutput(address);
      outputs[i]->setupdate_calback([](byte address, bool newPowerState) {
        pccwdController.setOnSate(address, newPowerState);
      });
      of8wdCount++;
    }
  }
  Serial.printf("OF8Wd outputs created: %d of %d enabled\n", of8wdCount, NUM_OF8WD);

  // Create IN10Wd inputs (stateless programmable switches) - only if enabled in config
  // Fixed IDs: 2-11 (AID_IN10WD_BASE + index)
  int in10wdCount = 0;
  for (int i = 0; i < NUM_IN10WD; i++) {
    if (configManager.isIN10WdEnabled(i)) {
      const HardcodedDeviceConfig& cfg = configManager.getIN10WdConfig(i);
      uint32_t aid = AID_IN10WD_BASE + i;
      snprintf(serialNum, sizeof(serialNum), "AID-%03d", aid);
      new SpanAccessory(aid);
      new AccessoryInfo(cfg.name.c_str(), "MORDOMUS", serialNum, "IN10wd", "0.1");
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

  // PWM LED - Fixed ID: 20
  if (configManager.isPwmLedEnabled()) {
    const HardcodedDeviceConfig& ledCfg = configManager.getPwmLedConfig();
    snprintf(serialNum, sizeof(serialNum), "AID-%03d", AID_PWM_LED);
    new SpanAccessory(AID_PWM_LED);
    pwmLedAccessoryInfo = new PWMLEDAccessoryInfo(ledCfg.name.c_str(), "MORDOMUS", serialNum, "PWM LED", "0.1", LED_PWM_PIN, 3);
    new DimmableLED(LED_PWM_PIN);
    Serial.println("PWM Dimmable LED created on GPIO2");
  } else {
    Serial.println("PWM Dimmable LED disabled in config");
  }

  // ============================================
  // MCP23017 GPIO Expander Accessories (configurable)
  // ============================================

  // Create accessories from configuration
  // MCP pins - Fixed IDs: 21-36 (AID_MCP_BASE + pin)
  // SecuritySystem - Fixed ID: 37
  if (mcpHandler.isConnected()) {
    pinFactory.createAccessories(configManager, mcpHandler, AID_MCP_BASE);
    Serial.println("MCP23017 accessories created");

    // Create SecuritySystem accessory if enabled
    pinFactory.createSecuritySystem(configManager, mcpHandler, AID_SECURITY);
  }

  Serial.println("=== Setup Complete ===\n");

} // end of setup()

//////////////////////////////////////

void loop() {
  // HomeSpan polling
  homeSpan.poll();

  // PCCWD bus polling
  pccwdController.loop();

  // OF8Wd identify blink polling
  for (int i = 0; i < NUM_OF8WD; i++) {
    if (of8wdAccessoryInfo[i]) {
      of8wdAccessoryInfo[i]->loop();
    }
  }

  // PWM LED identify blink polling
  if (pwmLedAccessoryInfo) {
    pwmLedAccessoryInfo->loop();
  }

  // Web server polling
  if (webServer) webServer->loop();

  // MCP23017 sensor polling (at reduced rate)
  unsigned long now = millis();
  if (now - lastSensorPoll >= SENSOR_POLL_INTERVAL) {
    lastSensorPoll = now;

    if (mcpHandler.isConnected()) {
      pinFactory.updateSensorStates();
    }
  }

} // end of loop()
