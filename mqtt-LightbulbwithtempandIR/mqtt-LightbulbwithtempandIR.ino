
// Includes //

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include "AM2320.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

// create a variable of sensor library
AM2320 sensor;


#define RELAY_Pin 12
#define MOTION_INPUT_PIN 14
#define IR_LED_PIN 13

#define MOTION_SAMPLES 3

IRsend irsend(IR_LED_PIN);

const char* mqtt_server = "192.168.1.109";
uint16_t i;
char serviceType[256] = "Lightbulb";

char pubTopic[256];
char pubMessage[256];

const char* mqttuser = "";
const char* mqttpass = "";

const char* intopic = "homebridge/from/set";
const char* outtopic = "homebridge/to/set";
const char* gettopic = "homebridge/from/get";
const char* addtopic = "homebridge/to/add";
const char* removetopic = "homebridge/to/remove";
const char* mainttopic = "homebridge/from/connected";
const char* servicetopic = "homebridge/to/add/service";
const char* reachabilitytopic = "homebridge/to/set/reachability";

const char* maintmessage = "";

bool lightBulbOn;

String chipId;
String chipIdAC;
String chipIdACFan;

String jsonReachabilityString;

float measuredTemperature = 0;
float measuredHumidity = 0;

//AC vars


// 0 : cooling
// 1 : heating
unsigned int ac_heat = 0;

// 0 : off
// 1 : on
unsigned int ac_power_on = 0;

// 0 : off
// 1 : on --> power on
unsigned int ac_air_clean_state = 0;

// temperature : 18 ~ 30
unsigned int ac_temperature = 22;

// 0 : low
// 1 : mid
// 2 : high
// if kAc_Type = 1, 3 : change
unsigned int ac_flow = 0;

unsigned int ac_swing_mode = 0;


const uint8_t kAc_Flow_Wall[4] = {0, 2, 4, 5};

uint32_t ac_code_to_sent;




//temp and humidity pooling variables
const unsigned long MeasureInterval = 2 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()



//motion stector stuff
const unsigned long MotionCheckInterval =  1000UL;


static unsigned long lastMotionCheckTime = 0 - MotionCheckInterval;
static bool MotionDetected = 0;
static bool MotionDetectedSample = 0;
static bool MotionDetectedPreviousSample = 0;
static int MotionDetectedAlignedSamples = 0;
static bool LastReportedMotionDetected = 0;
static bool MotionDetectionEnabled = 1;


WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);
  chipId = String(serviceType) + String(ESP.getChipId());
  chipIdAC = "AC" + String(ESP.getChipId());
  chipIdACFan = "ACFAN" + String(ESP.getChipId());

  pinMode(RELAY_Pin, OUTPUT);
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(MOTION_INPUT_PIN, INPUT);

  sensor.begin(4, 5);

  irsend.begin();

  lightBulbOn = true;
  digitalWrite(RELAY_Pin, lightBulbOn);

  StaticJsonBuffer<200> jsonReachabilityBuffer;
  JsonObject& jsonReachability = jsonReachabilityBuffer.createObject();
  jsonReachability["name"] = chipId.c_str();
  jsonReachability["reachable"] = false;
  jsonReachability.printTo(jsonReachabilityString);


  //wifi_conn();
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect(chipId.c_str());

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(chipId.c_str());
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  client.setServer(mqtt_server, 1883);

}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(chipId.c_str(), mqttuser, mqttpass, reachabilitytopic, 0, false, jsonReachabilityString.c_str())) {
        client.setCallback(callback);
        client.subscribe(intopic);
        client.subscribe(gettopic);
        //client.subscribe(mainttopic);
        addAccessory();
        setReachability();
        if (client.connected())
          Serial.println("connected to MQTT server");
      } else {
        Serial.println("Could not connect to MQTT server");
        delay(5000);
      }
    }

    if (client.connected())
      client.loop();
  } else {
    ESP.reset();
  }

  unsigned long now = millis();
  if (now - lastSampleTime >= MeasureInterval)
  {
    lastSampleTime += MeasureInterval;
    // sensor.measure() returns boolean value
    // - true indicates measurement is completed and success
    // - false indicates that either sensor is not ready or crc validation failed
    //   use getErrorCode() to check for cause of error.
    if (sensor.measure()) {
      Serial.print("Temperature: ");
      measuredTemperature = sensor.getTemperature();
      getAccessory(chipIdAC.c_str(), "AC", "CurrentTemperature");

      Serial.println(measuredTemperature);
      Serial.print("Humidity: ");
      measuredHumidity = sensor.getHumidity();
      getAccessory(chipIdAC.c_str(), "AC", "CurrentRelativeHumidity");
      getAccessory(chipId.c_str(), "Humidity Sensor", "CurrentRelativeHumidity");
      Serial.println(measuredHumidity);
    }
    else {  // error has occured
      int errorCode = sensor.getErrorCode();
      switch (errorCode) {
        case 1: Serial.println("ERR: Sensor is offline"); break;
        case 2: Serial.println("ERR: CRC validation failed."); break;
      }
    }
  }

  if (now - lastMotionCheckTime >= MotionCheckInterval)
  {
    lastMotionCheckTime = now;
    MotionDetectedSample = digitalRead(MOTION_INPUT_PIN) ;
    if (MotionDetectedAlignedSamples < MOTION_SAMPLES) {
      if (MotionDetectedSample != MotionDetectedPreviousSample) {
        MotionDetectedAlignedSamples = 0;
        MotionDetectedPreviousSample = MotionDetectedSample;
      } else {
        MotionDetectedAlignedSamples++;
      }
    } else {
      MotionDetectedAlignedSamples = 0;
      MotionDetectedPreviousSample = MotionDetectedSample;
      MotionDetected = MotionDetectedSample;
      if (MotionDetected != LastReportedMotionDetected && MotionDetectionEnabled) {
        LastReportedMotionDetected = MotionDetected;
        getAccessory(chipId.c_str(), "Motion Sensor", "MotionDetected");
        Serial.print("Motion Detected:");
        Serial.println(MotionDetected);
      }
    }
  }
}

void addAccessory() {
  StaticJsonBuffer<300> jsonLightbulbBuffer;
  JsonObject& addLightbulbAccessoryJson = jsonLightbulbBuffer.createObject();
  addLightbulbAccessoryJson["name"] = chipId.c_str();
  addLightbulbAccessoryJson["service"] = serviceType;
  addLightbulbAccessoryJson["service_name"] = "Lightbulb";
  addLightbulbAccessoryJson["reachable"] = true;
  String addLightbulbAccessoryJsonString;
  addLightbulbAccessoryJson.printTo(addLightbulbAccessoryJsonString);
  Serial.println(addLightbulbAccessoryJsonString.c_str());
  if (client.publish(addtopic, addLightbulbAccessoryJsonString.c_str()))
    Serial.println("Lightbulb Service Added");

  JsonObject& addThermostatJson = jsonLightbulbBuffer.createObject();
  addThermostatJson["name"] = chipIdAC.c_str();
  addThermostatJson["service"] = "Thermostat";
  addThermostatJson["service_name"] = "AC";
  addThermostatJson["CurrentRelativeHumidity"] = measuredHumidity;
  String addThermostatJsonString;
  addThermostatJson.printTo(addThermostatJsonString);
  Serial.println(addThermostatJsonString.c_str());
  if (client.publish(addtopic, addThermostatJsonString.c_str()))
    Serial.println("Thermostat Service Added");

  JsonObject& addfanJson = jsonLightbulbBuffer.createObject();
  addfanJson["name"] = chipIdACFan.c_str();
  addfanJson["service_name"] = "Fan";
  addfanJson["service"] = "Fanv2";
  addfanJson["SwingMode"] = ac_swing_mode;
  addfanJson["RotationSpeed"] = ac_flow / 2.0 * 100;
  String addfanJsonString;
  addfanJson.printTo(addfanJsonString);
  Serial.println(addfanJsonString.c_str());
  if (client.publish(addtopic, addfanJsonString.c_str()))
    Serial.println("fan Service Added");


  //{"name": "Master Sensor", "service_name": "humidity", "service": "HumiditySensor"}
  JsonObject& addHumidityServiceJson = jsonLightbulbBuffer.createObject();
  addHumidityServiceJson["name"] = chipId.c_str();
  addHumidityServiceJson["service_name"] = "Humidity Sensor";
  addHumidityServiceJson["service"] = "HumiditySensor";
  String addHumidityJsonString;
  addHumidityServiceJson.printTo(addHumidityJsonString);
  Serial.println(addHumidityJsonString.c_str());
  if (client.publish(servicetopic, addHumidityJsonString.c_str()))
    Serial.println("Humidity Service Added");

  //{"name": "Motion Sensor", "service_name": "Motion Sensor", "service": "MotionSensor"}
  JsonObject& addMotionSensorServiceJson = jsonLightbulbBuffer.createObject();
  addMotionSensorServiceJson["name"] = chipId.c_str();
  addMotionSensorServiceJson["service_name"] = "Motion Sensor";
  addMotionSensorServiceJson["service"] = "MotionSensor";
  String addMotionSensorServiceJsonString;
  addMotionSensorServiceJson.printTo(addMotionSensorServiceJsonString);
  Serial.println(addMotionSensorServiceJsonString.c_str());
  if (client.publish(servicetopic, addMotionSensorServiceJsonString.c_str()))
    Serial.println("Motion Service Added");
}

void maintAccessory() {
  Serial.println("Maintenance");
}

void setReachability() {
  StaticJsonBuffer<200> jsonSetReachability;
  JsonObject& setReachabilityJson = jsonSetReachability.createObject();
  setReachabilityJson["name"] = chipId.c_str();
  setReachabilityJson["reachable"] = true;
  jsonReachabilityString = "";
  setReachabilityJson.printTo(jsonReachabilityString);
  Serial.println(jsonReachabilityString);
  client.publish(reachabilitytopic, jsonReachabilityString.c_str());
}



void setAccessory(const char * accessoryName, const char * accessoryCharacteristic, const char *accessoryValue) {
  Serial.print("Set -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(" to ");
  Serial.println(accessoryValue);

  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipId) {
    lightBulbOn = (accessoryValue == std::string("true"));
    MotionDetectionEnabled = 0;
    digitalWrite(RELAY_Pin, lightBulbOn);
    delay(300);
    MotionDetectionEnabled = 1;
  }
  else  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipIdACFan) {

  }
  else if (accessoryCharacteristic == std::string("TargetHeatingCoolingState")) {

    switch (atoi(accessoryValue)) {
      case 0:        // turnOff
        Ac_Power_Down();
        break;
      case 1:        // Heat
        Ac_Activate(ac_temperature, ac_flow, 1);
        break;
      case 2:        // Cool
        Ac_Activate(ac_temperature, ac_flow, 0);
        break;
      default:        // not supposed to hapen
        Serial.print("UPS");
    };
  }
  else if (accessoryCharacteristic == std::string("TargetTemperature")) {
    ac_temperature = atoi(accessoryValue);
    if ( ac_temperature < 18)
      ac_temperature = 18;

    if (ac_temperature > 30)
      ac_temperature = 18;
    Ac_Activate(ac_temperature, ac_flow, ac_heat);
  }
  else if (accessoryCharacteristic == std::string("TemperatureDisplayUnits")) {

  }
  else if (accessoryCharacteristic == std::string("SwingMode")) {
    Ac_Change_Air_Swing(atoi(accessoryValue));
  }
  else if (accessoryCharacteristic == std::string("RotationSpeed")) {
    Ac_Activate(ac_temperature, abs(atof(accessoryValue) * 2.0 / 100), ac_heat);
  }



}

void getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {
  Serial.print("Get -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(": ");

  StaticJsonBuffer<200> jsonLightbulbBuffer;
  JsonObject& Json = jsonLightbulbBuffer.createObject();

  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;
  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipId) {
    Json["value"] = lightBulbOn;
    Serial.println(lightBulbOn);
  }
  else  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipIdACFan) {

  }
  else if (accessoryCharacteristic == std::string("CurrentTemperature")) {
    Json["value"] = measuredTemperature;
    Serial.println(measuredTemperature);
  }
  else if (accessoryCharacteristic == std::string("CurrentRelativeHumidity")) {
    Json["value"] = measuredHumidity;
    Serial.println(measuredHumidity);
  }
  else if (accessoryCharacteristic == std::string("MotionDetected")) {
    Json["value"] = MotionDetected;
    Serial.println(MotionDetected);
  }
  else if (accessoryCharacteristic == std::string("TargetHeatingCoolingState")) {
    if (!ac_power_on)
      Json["value"] = 0;
    else if (ac_heat == 0) {
      Json["value"] = 2;
    }
    else {
      Json["value"] = 1;
    }
    Serial.println(measuredTemperature);
  }
  else if (accessoryCharacteristic == std::string("TargetTemperature")) {
    Json["value"] = ac_temperature;
    Serial.println(ac_temperature);
  }
  else if (accessoryCharacteristic == std::string("SwingMode")) {
    Json["value"] = Ac_Change_Air_Swing;
  }
  else if (accessoryCharacteristic == std::string("RotationSpeed")) {
    Json["value"] = ac_flow / 2.0 * 100;
  }

  String UpdateJson;
  Json.printTo(UpdateJson);
  Serial.println(UpdateJson);
  client.publish(outtopic, UpdateJson.c_str());

}


void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  StaticJsonBuffer<512> jsoncallbackBuffer;
  JsonObject& mqttAccessory = jsoncallbackBuffer.parseObject(message);

  const char* accessoryName = mqttAccessory["name"];
  const char* accessoryServiceName = mqttAccessory["service_name"];

  if (mainttopic == std::string(topic)) {
    Serial.print("Maintenance");
    maintAccessory();
  }
  else if (intopic == std::string(topic)) {
    if (String(accessoryName) == chipId || String(accessoryName) == chipIdAC || String(accessoryName) == chipIdACFan) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      const char* accessoryValue = mqttAccessory["value"];
      setAccessory(accessoryName, accessoryCharacteristic, accessoryValue);
    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName) == chipId || String(accessoryName) == chipIdAC || String(accessoryName) == chipIdACFan) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      getAccessory(accessoryName, accessoryServiceName, accessoryCharacteristic);
    }
  }
}


void Ac_Send_Code(uint32_t code) {
  irsend.sendLG(code, 28);
}

void Ac_Activate(unsigned int temperature, unsigned int air_flow,
                 unsigned int heat) {
  ac_heat = heat;
  unsigned int ac_msbits1 = 8;
  unsigned int ac_msbits2 = 8;
  unsigned int ac_msbits3 = 0;
  unsigned int ac_msbits4;
  if (ac_heat == 1)
    ac_msbits4 = 4;  // heating
  else
    ac_msbits4 = 0;  // cooling
  unsigned int ac_msbits5 =  (temperature < 15) ? 0 : temperature - 15;
  unsigned int ac_msbits6;

  if (0 <= air_flow && air_flow <= 2) {
    ac_msbits6 = kAc_Flow_Wall[air_flow];
  }

  // calculating using other values
  unsigned int ac_msbits7 = (ac_msbits3 + ac_msbits4 + ac_msbits5 +
                             ac_msbits6) & B00001111;
  ac_code_to_sent = ac_msbits1 << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits2) << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits3) << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits4) << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits5) << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits6) << 4;
  ac_code_to_sent = (ac_code_to_sent + ac_msbits7);

  Ac_Send_Code(ac_code_to_sent);

  ac_power_on = 1;
  ac_temperature = temperature;
  ac_flow = air_flow;
  Serial.print("set ac_acivate");
  Serial.println(ac_code_to_sent);
}

void Ac_Change_Air_Swing(int air_swing) {
  if (air_swing == 1)
    ac_code_to_sent = 0x8813149;
  else
    ac_code_to_sent = 0x881315A;
  Ac_Send_Code(ac_code_to_sent);
  Serial.print("sent AC Swing:");
  Serial.println(ac_code_to_sent);
}

void Ac_Power_Down() {
  ac_code_to_sent = 0x88C0051;
  Ac_Send_Code(ac_code_to_sent);
  ac_power_on = 0;
  Serial.println("sent AC off:");

}

void Ac_Air_Clean(int air_clean) {
  if (air_clean == '1')
    ac_code_to_sent = 0x88C000C;
  else
    ac_code_to_sent = 0x88C0084;
  Ac_Send_Code(ac_code_to_sent);
  ac_air_clean_state = air_clean;
}



