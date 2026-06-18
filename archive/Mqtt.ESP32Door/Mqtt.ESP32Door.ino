#include <WiFiManager.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>


struct Doorbell {
  const uint8_t PIN;
  bool pressed;
};

volatile bool _pressed1;
volatile uint32_t debounceTimeout = 0;

Doorbell doorbell1 = {18, false};



const char* mqtt_server = "192.168.1.109";

char serviceType[256] = "Doorbell";


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

char chipId[23];
String TelemetryTopic;
String jsonReachabilityString;

void IRAM_ATTR isr() {
  if (xTaskGetTickCount() - debounceTimeout > 200) {
    _pressed1 = true;
    debounceTimeout = xTaskGetTickCount();
  }
}



WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);


  uint64_t mac = ESP.getEfuseMac();
  uint16_t chip = (uint16_t)(mac >> 32);


  snprintf(chipId, 23, "Doorbell-%04X%08X", chip, (uint32_t)mac);

  pinMode(doorbell1.PIN, INPUT_PULLUP);
  attachInterrupt(doorbell1.PIN, isr, FALLING);

  StaticJsonDocument<200> jsonReachability;
  jsonReachability["name"] = chipId;
  jsonReachability["reachable"] = true;
  serializeJson(jsonReachability, jsonReachabilityString);



  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect(chipId);

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(chipId);
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
  
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(chipId, mqttuser, mqttpass, reachabilitytopic, 0, false, jsonReachabilityString.c_str())) {
        client.setCallback(callback);
        client.subscribe(intopic);
        client.subscribe(gettopic);
        //client.subscribe(mainttopic);
       // addAccessory();
        //setReachability();
        if (client.connected())
          Serial.println("connected to MQTT server");
      } else {
        Serial.println("Could not connect to MQTT server");
        delay(5000);
      }
    } else {
      portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
      portENTER_CRITICAL(&mux);
      doorbell1.pressed = _pressed1;
      _pressed1 = false;
      portEXIT_CRITICAL(&mux);

      if (doorbell1.pressed) {
        //if (
          ProgrammableSwitchEvent();
          //)
          //Serial.println("doorbell press sent to mqtt");
        doorbell1.pressed = false;

        

      }

      client.loop();
    }
  }
  else {
    ESP.restart();
  }
  ArduinoOTA.handle();
}

void addAccessory() {
  StaticJsonDocument<800> addServiceJson;
  addServiceJson["name"] = chipId;
  addServiceJson["service_name"] = "Doorbell";
  addServiceJson["service"] = "Doorbell";//"MotionSensor";
  String addJsonString;
  serializeJson(addServiceJson, addJsonString);
  Serial.println(addJsonString.c_str());
  if (client.publish(addtopic, addJsonString.c_str()))
    Serial.println("Doorbell Service Added");
}

void setReachability() {
  StaticJsonDocument<200> jsonReachability;
  jsonReachability["name"] = chipId;
  jsonReachability["reachable"] = true;
  jsonReachabilityString = "";
  serializeJson(jsonReachability, jsonReachabilityString);
  
  client.publish(reachabilitytopic, jsonReachabilityString.c_str());
}

bool getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {

  StaticJsonDocument<300> Json;
  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;

  if (accessoryCharacteristic == std::string("FirmwareRevision")) {
    Json["value"] = "0.6.2";
  } 


  String UpdateJson;
  serializeJson(Json, UpdateJson);

  return client.publish(outtopic, UpdateJson.c_str());
  

}


bool ProgrammableSwitchEvent() {
  StaticJsonDocument<200> Json;
  Json["name"] = chipId;
  Json["service_name"] = "Doorbell";
  Json["characteristic"] = "ProgrammableSwitchEvent";
  Json["value"] = 0;

  String UpdateJson;
  serializeJson(Json, UpdateJson);
  
  return client.publish(outtopic, UpdateJson.c_str());
}


void callback(char* topic, byte * payload, unsigned int length) {
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  StaticJsonDocument<1024> mqttAccessory;
  DeserializationError error =  deserializeJson(mqttAccessory, message);
  if (error)
    return;

  const char* accessoryName = mqttAccessory["name"];
  const char* accessoryServiceName = mqttAccessory["service_name"];

  if (intopic == std::string(topic)) {
    if (String(accessoryName) == chipId ) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];

      Serial.print("Set -> ");
      Serial.print(accessoryCharacteristic);
      Serial.print(" to ");
      Serial.println((int)mqttAccessory["value"]);

    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName) == chipId ) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      Serial.print("Get -> ");
      Serial.println(accessoryCharacteristic);
      getAccessory(accessoryName, accessoryServiceName, accessoryCharacteristic);
    }
  }
}
