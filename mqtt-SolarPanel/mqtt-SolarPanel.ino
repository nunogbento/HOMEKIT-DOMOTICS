

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* mqtt_server = "192.168.1.109";

char serviceType[256] = "SolarPanel";


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
String TelemetryTopic;
String jsonReachabilityString;

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 14

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

float measuredTemperature = 0;
int HeaterCoolerState = 0;

Adafruit_ADS1115 ads;

const float FACTOR = 60.6F; //100A/50ma   33ohm burden  100A/1.65v

const float multiplier = 0.125;

// pooling variables
const unsigned long MeasureInterval = 2 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()


WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);
  chipId = String(serviceType) + String(ESP.getChipId());
  TelemetryTopic=chipId+"/from";
  sensors.begin();

  ads.setGain(GAIN_ONE);        // 0.125mv
  //ads.begin();

  Wire.begin( 5,4);

  StaticJsonDocument<200> jsonReachability;
  jsonReachability["name"] = chipId.c_str();
  jsonReachability["reachable"] = false;
  serializeJson(jsonReachability, jsonReachabilityString);

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
    StaticJsonDocument<100> powerStatusJson;
    HeaterCoolerState = 0;
    
    sensors.requestTemperatures(); // Send the command to get temperatures
    measuredTemperature = sensors.getTempCByIndex(0);
    getAccessory(chipId.c_str(), "SolarPanel", "CurrentTemperature");
    powerStatusJson["temp"] = measuredTemperature;

    float currentRMS = getAmps(0);
    if (currentRMS > 1)HeaterCoolerState = 2;
    powerStatusJson["iCh1"] = currentRMS;   
    
    currentRMS = getAmps(1);
    if (currentRMS > 0.3F)HeaterCoolerState = 1;
    powerStatusJson["iCh2"] = currentRMS;
    getAccessory(chipId.c_str(), "SolarPanel", "CurrentHeaterCoolerState");
    String powerStatusJsonString;
    serializeJson(powerStatusJson, powerStatusJsonString);
    Serial.println(powerStatusJsonString.c_str());
    client.publish(TelemetryTopic.c_str(), powerStatusJsonString.c_str());
  }
}

void addAccessory() {
  StaticJsonDocument<800> addServiceJson;
  addServiceJson["name"] = chipId.c_str();
  addServiceJson["service_name"] = "SolarPanel";
  addServiceJson["service"] = "Thermostat";
  String addJsonString;

  serializeJson(addServiceJson, addJsonString);
  Serial.println(addJsonString.c_str());
  if (client.publish(addtopic, addJsonString.c_str()))
    Serial.println("Solar Panel Service Added");

}

void maintAccessory() {
  Serial.println("Maintenance");
}

void setReachability() {
  StaticJsonDocument<200> jsonReachability;

  jsonReachability["name"] = chipId.c_str();
  jsonReachability["reachable"] = true;
  jsonReachabilityString = "";
  serializeJson(jsonReachability, jsonReachabilityString);

  Serial.println(jsonReachabilityString);
  client.publish(reachabilitytopic, jsonReachabilityString.c_str());
}




void getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {
  Serial.print("Get -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(": ");

  StaticJsonDocument<200> Json;


  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;

  if (accessoryCharacteristic == std::string("Active")) {
    Json["value"] = true;
  }
  else if (accessoryCharacteristic == std::string("CurrentTemperature")) {
    Json["value"] = measuredTemperature;
  }
  else if (accessoryCharacteristic == std::string("CurrentHeaterCoolerState")) {
    Json["value"] = HeaterCoolerState;
  }
  else if (accessoryCharacteristic == std::string("TargetHeaterCoolerState")) {
    Json["value"] = 30;
  }
  else if (accessoryCharacteristic == std::string("FirmwareRevision")) {
    Json["value"] = "0.6.2";
  }

  String UpdateJson;

  serializeJson(Json, UpdateJson);
  Serial.println(UpdateJson);
  client.publish(outtopic, UpdateJson.c_str());

}


void callback(char* topic, byte* payload, unsigned int length) {
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

  if (mainttopic == std::string(topic)) {
    Serial.print("Maintenance");
    maintAccessory();
  }
  else if (intopic == std::string(topic)) {
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
      getAccessory(accessoryName, accessoryServiceName, accessoryCharacteristic);
    }
  }
}

float getAmps(uint8_t channel)
{
  float voltage;
  float amps;
  float sum = 0;
  long startMillis = millis();
  int counter = 0;

  while (millis() - startMillis < 1000)
  {
    if (channel == 0)
      voltage = ads.readADC_Differential_0_1() * multiplier;
    else
      voltage = ads.readADC_Differential_2_3() * multiplier;
    amps = voltage * FACTOR;
    amps /= 1000.0;

    sum += sq(amps);
    counter++;
  }

  
  amps = sqrt(sum / counter);
  return amps;
}
