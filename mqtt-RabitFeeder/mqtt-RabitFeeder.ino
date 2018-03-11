



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
#include <NTPClient.h>
#include <Servo.h>
#include <EEPROM.h>

// create a variable of sensor library
AM2320 sensor;


#define LDRPin D6
#define ServoPWMPin D7

#define AUTO_FEED_ADDRESS 0
#define LAST_FEED_TIME_ADDRESS 0

const char* mqtt_server = "192.168.1.109";
uint16_t i;
char serviceType[256] = "Switch";

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

String chipId;
String jsonReachabilityString;
float measuredTemperature = 0;
float measuredHumidity = 0;

volatile bool Aligned = 0;
bool MovingToNextSlot = 0;
bool AutoFeed;
bool Feed;

//temp and humidity pooling variables
const unsigned long MeasureInterval = 2 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()

WiFiClient wclient;
PubSubClient client(wclient);


WiFiUDP ntpUDP;


//init servo library object
Servo servo;

// By default 'time.nist.gov' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);

void handleInterrupt() {
  Aligned = 1;
}

void setup() {
  Serial.begin(74880);
  EEPROM.begin(512);
  chipId = String(serviceType) + String(ESP.getChipId());

  pinMode(ServoPWMPin, OUTPUT);
  pinMode(LDRPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LDRPin), handleInterrupt, FALLING);
  analogWriteRange(255);
  sensor.begin();
  timeClient.begin();
  StaticJsonBuffer<200> jsonReachabilityBuffer;
  JsonObject& jsonReachability = jsonReachabilityBuffer.createObject();
  jsonReachability["name"] = chipId.c_str();
  jsonReachability["reachable"] = false;
  jsonReachability.printTo(jsonReachabilityString);
  //setup servo motor
  servo.detach();

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

  timeClient.update();

  //Serial.println(timeClient.getFormattedTime());


  if (Aligned) {
    Aligned = 0;
    Feed=0;
    //stop moving
    Serial.println("aligned");
    servo.detach();
    getAccessory("Feed Now Switch", "On");
    String time = timeClient.getFormattedTime();
    Serial.print("Rabit Feed at:");
    Serial.println(time);
    writeStringToEEPROM(time, LAST_FEED_TIME_ADDRESS);

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
      getAccessory("Temperature Sensor", "CurrentTemperature");
      Serial.println(measuredTemperature);
      Serial.print("Humidity: ");
      measuredHumidity = sensor.getHumidity();
      getAccessory("Humidity Sensor", "CurrentRelativeHumidity");
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
}

void addAccessory() {
  StaticJsonBuffer<300> jsonLightbulbBuffer;
  JsonObject& addSwitchAccessoryJson = jsonLightbulbBuffer.createObject();
  addSwitchAccessoryJson["name"] = chipId.c_str();
  addSwitchAccessoryJson["service"] = serviceType;
  addSwitchAccessoryJson["service_name"] = "Feed Now Switch";
  addSwitchAccessoryJson["reachable"] = true;
  String addSwitchAccessoryJsonString;
  addSwitchAccessoryJson.printTo(addSwitchAccessoryJsonString);
  Serial.println(addSwitchAccessoryJsonString.c_str());
  if (client.publish(addtopic, addSwitchAccessoryJsonString.c_str()))
    Serial.println("Manual Feed Service Added");

  //{"name": "Master Sensor", "service_name": "Auto Feed Switch", "service": "Switch"}
  JsonObject& addSwitchServiceJson = jsonLightbulbBuffer.createObject();
  addSwitchServiceJson["name"] = chipId.c_str();
  addSwitchServiceJson["service_name"] = "Auto Feed Switch";
  addSwitchServiceJson["service"] = "Switch";
  String addSwitchServiceJsonString;
  addSwitchServiceJson.printTo(addSwitchServiceJsonString);
  Serial.println(addSwitchServiceJsonString.c_str());
  if (client.publish(servicetopic, addSwitchServiceJsonString.c_str()))
    Serial.println("Auto Feed Service Added");


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

  //{"name": "Master Sensor", "service_name": "temperature", "service": "TemperatureSensor"}
  JsonObject& addTemperatureServiceJson = jsonLightbulbBuffer.createObject();
  addTemperatureServiceJson["name"] = chipId.c_str();
  addTemperatureServiceJson["service_name"] = "Temperature Sensor";
  addTemperatureServiceJson["service"] = "TemperatureSensor";
  String addTemperatureJsonString;
  addTemperatureServiceJson.printTo(addTemperatureJsonString);
  Serial.println(addTemperatureJsonString.c_str());
  if (client.publish(servicetopic, addTemperatureJsonString.c_str()))
    Serial.println("Temperature Service Added");


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



void setAccessory(const char * accessoryServiceName, const char * accessoryCharacteristic, const char *accessoryValue) {
  Serial.print("Set -> ");
  Serial.print(accessoryServiceName);
  Serial.print(" -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(" to ");
  Serial.println(accessoryValue);

  if (accessoryCharacteristic == std::string("On")) {
    if (accessoryServiceName == std::string("Auto Feed Switch") ) {
      byte value = (accessoryValue == std::string("true"));
      EEPROM.write(AUTO_FEED_ADDRESS, value);
    } else {
      Feed = (accessoryValue == std::string("true"));
      if (Feed) {
        servo.attach(ServoPWMPin);
        //stop servo, not sure if needed. I didn't manage to prevent the servo from
        // slightly turning when powered on the first time.
        servo.write(110);
      }
    }



  }
}

void getAccessory(const char * accessoryServiceName, const char * accessoryCharacteristic) {
  Serial.print("Get -> ");
  Serial.print(accessoryServiceName);
  Serial.print(" -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(": ");
  //{"name":"{{payload.Id}}","service_name": "PB","characteristic":"ProgrammableSwitchEvent","value":{{payload.Count}}}
  StaticJsonBuffer<200> jsonLightbulbBuffer;
  JsonObject& Json = jsonLightbulbBuffer.createObject();
  Json["name"] = chipId.c_str();
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;
  if (accessoryCharacteristic == std::string("On")) {
    if (accessoryServiceName == std::string("Auto Feed Switch") ) {
      byte value = EEPROM.read(AUTO_FEED_ADDRESS);
      Json["value"] = (value == 1);
      Serial.println(value == 1);
    } else {
      Json["value"] = Feed;
      Serial.println(Feed);
    }
  } else if (accessoryCharacteristic == std::string("CurrentTemperature")) {
    Json["value"] = measuredTemperature;
    Serial.println(measuredTemperature);
  }
  else if (accessoryCharacteristic == std::string("CurrentRelativeHumidity")) {
    Json["value"] = measuredHumidity;
    Serial.println(measuredHumidity);
  }

  String UpdateJson;
  Json.printTo(UpdateJson);
  Serial.println(UpdateJson);
  client.publish(outtopic, UpdateJson.c_str());

}


void callback(char* topic, byte * payload, unsigned int length) {

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
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      const char* accessoryValue = mqttAccessory["value"];
      setAccessory(accessoryServiceName, accessoryCharacteristic, accessoryValue);
    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      getAccessory(accessoryServiceName, accessoryCharacteristic);
    }
  }

}

String readStringFromEEPROM(int l, int p) {
  String temp;
  for (int n = p; n < l + p; ++n)
  {
    if (char(EEPROM.read(n)) != ';') {
      if (isWhitespace(char(EEPROM.read(n)))) {
        //do nothing
      } else temp += String(char(EEPROM.read(n)));

    } else n = l + p;

  }
  return temp;
}

void writeStringToEEPROM(String x, int pos) {
  for (int n = pos; n < x.length() + pos; n++) {
    EEPROM.write(n, x[n - pos]);
  }
}

