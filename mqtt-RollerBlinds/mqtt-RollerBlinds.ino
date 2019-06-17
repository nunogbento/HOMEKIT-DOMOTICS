
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


#define BUTTON_1_Pin 0
#define BUTTON_2_Pin 9
#define RELAY_1_Pin 12
#define RELAY_2_Pin 5
#define FullRunMillSecs 26000
#define STATUS_LED_PIN 13

const char* mqtt_server = "192.168.1.109";
uint16_t i;

char serviceType[256] = "WindowCovering";

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

int PositionState;
int TargetPosition;
int CurrentPosition;
int LastPosition;

const unsigned long MeasureInterval = 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()
unsigned long elapsedTime = 0;

const unsigned long DebounceInterval = 250UL;
static unsigned long lastButtonPress = 0 - DebounceInterval;  // initialize such that a reading is due the first time through loop()

String chipId;
String jsonReachabilityString;

WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);
  chipId = String(serviceType) + String(ESP.getChipId());

  pinMode(RELAY_1_Pin, OUTPUT);
  pinMode(RELAY_2_Pin, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  pinMode(BUTTON_1_Pin, INPUT);
  pinMode(BUTTON_2_Pin, INPUT);
  

  digitalWrite(RELAY_1_Pin, 0);
  digitalWrite(RELAY_2_Pin, 0);
  digitalWrite(STATUS_LED_PIN, 1);
  

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
          digitalWrite(STATUS_LED_PIN, 0);
      } else {
        Serial.println("Could not connect to MQTT server");
        digitalWrite(STATUS_LED_PIN, 1);
        delay(5000);        
      }      
    }

    if (client.connected())
      client.loop();
  } //else {
    //ESP.reset();
  //}

  unsigned long now = millis();

  if (digitalRead(BUTTON_1_Pin)  == false && (now - lastButtonPress) > DebounceInterval ) {
    lastButtonPress = now;
    if (PositionState == 2)
      SetTargetPosition(0);
    else
      SetTargetPosition(CurrentPosition);
    getAccessory(chipId.c_str(), "WindowCovering", "TargetPosition");
  }

  if (digitalRead(BUTTON_2_Pin) == false && (now - lastButtonPress) > DebounceInterval) {
    lastButtonPress = now;
    if (PositionState == 2)
      SetTargetPosition(100);
    else
      SetTargetPosition(CurrentPosition);
    getAccessory(chipId.c_str(), "WindowCovering", "TargetPosition");
  }

  elapsedTime = now - lastSampleTime;

  if (elapsedTime >= MeasureInterval) {
    //update current Position
    lastSampleTime = now;
    if (PositionState == 0) {//going down
      CurrentPosition -= (100 * elapsedTime / FullRunMillSecs);
      if (CurrentPosition <= TargetPosition)
      {
        CurrentPosition = TargetPosition;
        SetPositionState(2);
      }
    }
    else if (PositionState == 1) {//going up
      CurrentPosition += (100 * elapsedTime / FullRunMillSecs);
      if (CurrentPosition >= TargetPosition)
      {
        CurrentPosition = TargetPosition;
        SetPositionState(2);
      }
    }
    if (CurrentPosition != LastPosition) {
      LastPosition=CurrentPosition;
      Serial.print("CurrentPosition:");
      Serial.println(CurrentPosition);
      getAccessory(chipId.c_str(), "WindowCovering", "CurrentPosition");
    }
  }
}



void SetTargetPosition(int targetPosition) {
  TargetPosition = targetPosition;
  Serial.print("TargetPosition Set:");
  Serial.println(TargetPosition);
  if (targetPosition > CurrentPosition) {
    SetPositionState(1);
  } else  if (targetPosition < CurrentPosition) {
    SetPositionState(0);
  } else {
    SetPositionState(2);
  }
}

void SetPositionState(int positionState) {
  PositionState = positionState;
  if (PositionState == 0) {
    digitalWrite(RELAY_2_Pin, 0);
    delay(20);
    digitalWrite(RELAY_1_Pin, 1);
    Serial.println("Going Down");
  }
  else if (PositionState == 1) {
    digitalWrite(RELAY_1_Pin, 0);
    delay(20);
    digitalWrite(RELAY_2_Pin, 1);
    Serial.println("Going Up");
  }
  else if (PositionState == 2) {
    digitalWrite(RELAY_1_Pin, 0);
    digitalWrite(RELAY_2_Pin, 0);
    Serial.println("Stopped");
  }
  getAccessory(chipId.c_str(), "WindowCovering", "PositionState");
}

void addAccessory() {
  StaticJsonBuffer<300> jsonLightbulbBuffer;
  JsonObject& addLightbulbAccessoryJson = jsonLightbulbBuffer.createObject();
  addLightbulbAccessoryJson["name"] = chipId.c_str();
  addLightbulbAccessoryJson["service"] = serviceType;
  addLightbulbAccessoryJson["service_name"] = "WindowCovering";
  addLightbulbAccessoryJson["reachable"] = true;
  String addLightbulbAccessoryJsonString;
  addLightbulbAccessoryJson.printTo(addLightbulbAccessoryJsonString);
  //Serial.println(addLightbulbAccessoryJsonString.c_str());
  if (client.publish(addtopic, addLightbulbAccessoryJsonString.c_str()))
    Serial.println("WindowCovering Service Added");

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

  if (accessoryCharacteristic == std::string("TargetPosition")) {
    SetTargetPosition(atoi(accessoryValue));
  }
}

void getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {
//  Serial.print("Get -> ");
//  Serial.print(accessoryCharacteristic);
//  Serial.print(": ");
//  Serial.println(accessoryCharacteristic);
  
  StaticJsonBuffer<200> jsonLightbulbBuffer;
  JsonObject& Json = jsonLightbulbBuffer.createObject();

  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;

  if (accessoryCharacteristic == std::string("PositionState") ) {
    Json["value"] = PositionState;
  }
  else  if (accessoryCharacteristic == std::string("TargetPosition") ) {
    Json["value"] = TargetPosition;
  }
  else if (accessoryCharacteristic == std::string("CurrentPosition")) {
    Json["value"] = CurrentPosition;
  }

  String UpdateJson;
  Json.printTo(UpdateJson);
  //Serial.println(UpdateJson);
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
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      const char* accessoryValue = mqttAccessory["value"];
      setAccessory(accessoryName, accessoryCharacteristic, accessoryValue);
    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      getAccessory(accessoryName, accessoryServiceName, accessoryCharacteristic);
    }
  }
}




