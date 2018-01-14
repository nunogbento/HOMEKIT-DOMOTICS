
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

// create a variable of sensor library
AM2320 sensor;

//Led supports RGB
#define IsRGB false
//led is only white
#define IsWHITE true

#define WHITE_LedPin D8
#define RED_LedPin 15
#define GREEN_LedPin 15
#define BLUE_LedPin 15


#define MOTION_INPUT_PIN_1 D7
#define MOTION_INPUT_PIN_2 D6
#define MOTION_SAMPLES 5

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
u_int lightBrightness;
u_int lightHue;
u_int lightSaturation;

String chipId;
String jsonReachabilityString;

float measuredTemperature = 0;
float measuredHumidity = 0;

// RealPWM values to write to the LEDs (ex. including brightness and state)
byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;
byte realWhite = 0;

// Globals for fade/transitions
bool startFade = false;
unsigned long lastLoop = 0;
int transitionTime = 1;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB, stepW;
int redVal, grnVal, bluVal, whtVal;

//temp and humidity pooling variables
const unsigned long MeasureInterval = 2 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()



//motion stector stuff
const unsigned long MotionCheckInterval =  100UL;


static unsigned long lastMotionCheckTime = 0 - MotionCheckInterval;
static bool MotionDetected = 0;
static bool MotionDetectedSample = 0;
static bool MotionDetectedPreviousSample = 0;
static int MotionDetectedAlignedSamples = 0;
static bool LastReportedMotionDetected = 0;

WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(74880);
  chipId = String(serviceType) + String(ESP.getChipId());

  if (IsWHITE) {
    pinMode(WHITE_LedPin, OUTPUT);
  }
  if (IsRGB) {
    pinMode(RED_LedPin, OUTPUT);
    pinMode(GREEN_LedPin, OUTPUT);
    pinMode(BLUE_LedPin, OUTPUT);
  }

  pinMode(MOTION_INPUT_PIN_1, INPUT);
  pinMode(MOTION_INPUT_PIN_2, INPUT);

  analogWriteRange(255);

  sensor.begin();

  lightBulbOn = true;
  lightBrightness = 255;
  lightHue = 0;
  lightSaturation = 0;


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

  if (now - lastMotionCheckTime >= MotionCheckInterval)
  {
    lastMotionCheckTime += MotionCheckInterval;
    MotionDetectedSample = digitalRead(MOTION_INPUT_PIN_1) || digitalRead(MOTION_INPUT_PIN_2);



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
      if (MotionDetected != LastReportedMotionDetected) {
        LastReportedMotionDetected = MotionDetected;
        getAccessory("Motion Sensor", "MotionDetected");
        Serial.print("Motion Detected:");
        Serial.println(MotionDetected);
      }
    }




  }

  if (startFade) {
    startFade = false;
    if (IsWHITE )
      realWhite = (lightBulbOn) ?  map(lightBrightness, 0, 100, 255, 0) : 255;
    Serial.println(String(lightBulbOn) + String(" --> rw=") + String(realWhite));
    //if(IsRGB)
    //todo map HSL to RGB

    // If we don't want to fade, skip it.
    if (transitionTime == 0) {
      setColor(realRed, realGreen, realBlue, realWhite);

      if (IsWHITE) {
        whtVal = realWhite;
      }
      if (IsRGB) {
        redVal = realRed;
        grnVal = realGreen;
        bluVal = realBlue;
      }
    }
    else {
      loopCount = 0;
      if (IsWHITE) {
        stepW = calculateStep(whtVal, realWhite);
      }
      if (IsRGB) {
        stepR = calculateStep(redVal, realRed);
        stepG = calculateStep(grnVal, realGreen);
        stepB = calculateStep(bluVal, realBlue);
      }
      inFade = true;
    }
  }

  if (inFade) {
    startFade = false;
    unsigned long now = millis();
    if (now - lastLoop > transitionTime) {
      if (loopCount <= 1020) {
        lastLoop = now;
        if (IsRGB) {
          redVal = calculateVal(stepR, redVal, loopCount);
          grnVal = calculateVal(stepG, grnVal, loopCount);
          bluVal = calculateVal(stepB, bluVal, loopCount);
        }
        if (IsWHITE) {
          whtVal = calculateVal(stepW, whtVal, loopCount);
        }
        setColor(redVal, grnVal, bluVal, whtVal); // Write current values to LED pins

        //Serial.print("Loop count: ");
        //Serial.println(loopCount);
        loopCount++;
      }
      else {
        inFade = false;
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
  addLightbulbAccessoryJson["Brightness"] = lightBrightness;
  if (IsRGB) {
    addLightbulbAccessoryJson["Hue"] = lightHue;
    addLightbulbAccessoryJson["Saturation"] = lightSaturation;
  }
  addLightbulbAccessoryJson["reachable"] = true;
  String addLightbulbAccessoryJsonString;
  addLightbulbAccessoryJson.printTo(addLightbulbAccessoryJsonString);
  Serial.println(addLightbulbAccessoryJsonString.c_str());
  if (client.publish(addtopic, addLightbulbAccessoryJsonString.c_str()))
    Serial.println("Lightbulb Service Added");

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

  //{"name": "Master Sensor", "service_name": "Motion Sensor", "service": "MotionSensor"}
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



void setAccessory(const char * accessoryCharacteristic, const char *accessoryValue) {
  Serial.print("Set -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(" to ");
  Serial.println(accessoryValue);

  if (accessoryCharacteristic == std::string("On")) {
    lightBulbOn = (accessoryValue == std::string("true"));
  }
  else if (accessoryCharacteristic == std::string("Brightness")) {
    lightBrightness = atoi(accessoryValue);
  }
  else if (accessoryCharacteristic == std::string("Hue")) {
    lightHue = atoi(accessoryValue);;
  }
  else if (accessoryCharacteristic == std::string("Saturation")) {
    lightSaturation = atoi(accessoryValue);;
  }

  startFade = true;
  inFade = false; // Kill the current fade
}

void getAccessory(const char * accessoryServiceName, const char * accessoryCharacteristic) {
  Serial.print("Get -> ");
  Serial.print(accessoryCharacteristic);
  Serial.print(": ");
  //{"name":"{{payload.Id}}","service_name": "PB","characteristic":"ProgrammableSwitchEvent","value":{{payload.Count}}}
  StaticJsonBuffer<200> jsonLightbulbBuffer;
  JsonObject& Json = jsonLightbulbBuffer.createObject();
  Json["name"] = chipId.c_str();
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;
  if (accessoryCharacteristic == std::string("On")) {
    Json["value"] = lightBulbOn;
    Serial.println(lightBulbOn);
  }
  else if (accessoryCharacteristic == std::string("Brightness")) {
    Json["value"] = lightBrightness;
    Serial.println(lightBrightness);
  }
  else if (accessoryCharacteristic == std::string("Hue")) {
    Json["value"] = lightHue;
    Serial.println(lightHue);
  }
  else if (accessoryCharacteristic == std::string("Saturation")) {
    Json["value"] = lightSaturation;
    Serial.println(lightSaturation);
  } else if (accessoryCharacteristic == std::string("CurrentTemperature")) {
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
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      const char* accessoryValue = mqttAccessory["value"];
      setAccessory(accessoryCharacteristic, accessoryValue);
    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName) == chipId) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      getAccessory(accessoryServiceName, accessoryCharacteristic);
    }
  }

}

int calculateStep(int prevValue, int endValue) {
  int step = endValue - prevValue; // What's the overall gap?
  if (step) {                      // If its non-zero,
    step = 1020 / step;          //   divide by 1020
  }

  return step;
}

int calculateVal(int step, int val, int i) {
  if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
    if (step > 0) {              //   increment the value if step is positive...
      val += 1;
    }
    else if (step < 0) {         //   ...or decrement it if step is negative
      val -= 1;
    }
  }

  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255) {
    val = 255;
  }
  else if (val < 0) {
    val = 0;
  }

  return val;
}

void setColor(int inR, int inG, int inB, int inW) {
  if (IsRGB) {
    analogWrite(RED_LedPin, inR);
    analogWrite(GREEN_LedPin, inG);
    analogWrite(BLUE_LedPin, inB);
  }
  if (IsWHITE) {
    analogWrite(WHITE_LedPin, inW);
  }
  Serial.print("Setting LEDs: {");
  if (IsRGB) {
    Serial.print("r: ");
    Serial.print(inR);
    Serial.print(" , g: ");
    Serial.print(inG);
    Serial.print(" , b: ");
    Serial.print(inB);
  }

  if (IsWHITE) {
    if (IsRGB) {
      Serial.print(", ");
    }
    Serial.print("w: ");
    Serial.print(inW);
  }
  Serial.println("}");

}
