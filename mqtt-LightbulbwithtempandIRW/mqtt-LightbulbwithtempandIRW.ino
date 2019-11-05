
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
#define STRIP_MODE W;//"RGBW" //W;CW;RGB;RGBW
#define WHITE_LedPin 13
#define RED_LedPin 16
#define GREEN_LedPin 14
#define BLUE_LedPin 12

#define DEG_TO_RAD(X) (M_PI*(X)/180)
//#define MOTION_INPUT_PIN 14
//#define MOTION_INPUT_PIN 9
#define IR_LED_PIN 10

//#define MOTION_SAMPLES 3

IRsend irsend(IR_LED_PIN);

const char* mqtt_server = "192.168.1.109";
uint16_t i;
char serviceType[256] = "Lightbulb";

//char pubTopic[256];
//char pubMessage[256];

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
u_int lightBulbBrightness;
u_int lightBulbHue;
u_int lightBulbSaturation;

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
//const unsigned long MotionCheckInterval =  1000UL;


//static unsigned long lastMotionCheckTime = 0 - MotionCheckInterval;
//static bool MotionDetected = 0;
//static bool MotionDetectedSample = 0;
//static bool MotionDetectedPreviousSample = 0;
//static int MotionDetectedAlignedSamples = 0;
//static bool LastReportedMotionDetected = 0;



WiFiClient wclient;
PubSubClient client(wclient);

void setup() {
  Serial.begin(115200);
  chipId = String(serviceType) + String(ESP.getChipId());
  chipIdAC = "AC" + String(ESP.getChipId());
  chipIdACFan = "ACFAN" + String(ESP.getChipId());

  pinMode(WHITE_LedPin, OUTPUT);
  pinMode(RED_LedPin, OUTPUT);
  pinMode(GREEN_LedPin, OUTPUT);
  pinMode(BLUE_LedPin, OUTPUT);

  pinMode(IR_LED_PIN, OUTPUT);
  //pinMode(MOTION_INPUT_PIN, INPUT);

  sensor.begin(4, 5);

  irsend.begin();
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
    // sensor.measure() returns boolean value
    // - true indicates measurement is completed and success
    // - false indicates that either sensor is not ready or crc validation failed
    //   use getErrorCode() to check for cause of error.
    if (sensor.measure()) {
      Serial.print("Temperature: ");
      measuredTemperature = sensor.getTemperature();
      getAccessory(chipIdAC.c_str(), "AC", "CurrentTemperature");
      // getAccessory(chipId.c_str(), "Temperature Sensor", "CurrentTemperature");
      Serial.println(measuredTemperature);
      Serial.print("Humidity: ");
      measuredHumidity = sensor.getHumidity();
      getAccessory(chipIdAC.c_str(), "AC", "CurrentRelativeHumidity");
      //getAccessory(chipId.c_str(), "Humidity Sensor", "CurrentRelativeHumidity");
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

  //  if (now - lastMotionCheckTime >= MotionCheckInterval)
  //  {
  //    lastMotionCheckTime = now;
  //    int reading = analogRead(A0);
  //    Serial.print("reading:");
  //    Serial.println(reading);
  //    MotionDetectedSample = reading>20;//digitalRead(MOTION_INPUT_PIN) ;
  //    if (MotionDetectedAlignedSamples < MOTION_SAMPLES) {
  //      if (MotionDetectedSample != MotionDetectedPreviousSample) {
  //        MotionDetectedAlignedSamples = 0;
  //        MotionDetectedPreviousSample = MotionDetectedSample;
  //      } else {
  //        MotionDetectedAlignedSamples++;
  //      }
  //    } else {
  //      MotionDetectedAlignedSamples = 0;
  //      MotionDetectedPreviousSample = MotionDetectedSample;
  //      MotionDetected = MotionDetectedSample;
  //      if (MotionDetected != LastReportedMotionDetected) {
  //        LastReportedMotionDetected = MotionDetected;
  //        getAccessory(chipId.c_str(), "Motion Sensor", "MotionDetected");
  //        Serial.print("Motion Detected:");
  //        Serial.println(MotionDetected);
  //      }
  //    }
  //  }
}

void addAccessory() {
  StaticJsonDocument<800> addLightbulbAccessoryJson;


  addLightbulbAccessoryJson["name"] = chipId.c_str();
  addLightbulbAccessoryJson["service_name"] = "Lightbulb";
  addLightbulbAccessoryJson["service"] = serviceType;

  addLightbulbAccessoryJson["Brightness"] = lightBulbBrightness;
//  addLightbulbAccessoryJson["Hue"] = lightBulbHue;
//  addLightbulbAccessoryJson["Saturation"] = lightBulbSaturation;
  String addLightbulbAccessoryJsonString;

  serializeJson(addLightbulbAccessoryJson, addLightbulbAccessoryJsonString);

  Serial.println(addLightbulbAccessoryJsonString.c_str());
  if (client.publish(addtopic, addLightbulbAccessoryJsonString.c_str()))
    Serial.println("Lightbulb Service Added");

  //{"name": "Master Sensor", "service_name": "humidity", "service": "HumiditySensor"}

  StaticJsonDocument<500> addHumidityServiceJson;
  addHumidityServiceJson["name"] = chipId.c_str();
  addHumidityServiceJson["service_name"] = "Humidity Sensor";
  addHumidityServiceJson["service"] = "HumiditySensor";
  String addHumidityJsonString;

  serializeJson(addHumidityServiceJson, addHumidityJsonString);
  Serial.println(addHumidityJsonString.c_str());
  if (client.publish(servicetopic, addHumidityJsonString.c_str()))
    Serial.println("Humidity Service Added");


  //  StaticJsonDocument<500> addTemperatureServiceJson;
  //  addTemperatureServiceJson["name"] = chipId.c_str();
  //  addTemperatureServiceJson["service_name"] = "Temperature Sensor";
  //  addTemperatureServiceJson["service"] = "TemperatureSensor";
  //  String addTemperatureJsonString;
  //
  //  serializeJson(addTemperatureServiceJson, addTemperatureJsonString);
  //  Serial.println(addTemperatureJsonString.c_str());
  //  if (client.publish(servicetopic, addTemperatureJsonString.c_str()))
  //    Serial.println("Temperature Service Added");

  //  //{"name": "Motion Sensor", "service_name": "Motion Sensor", "service": "MotionSensor"}
  //
  //  StaticJsonDocument<500> addMotionSensorServiceJson;
  //  addMotionSensorServiceJson["name"] = chipId.c_str();
  //  addMotionSensorServiceJson["service_name"] = "Motion Sensor";
  //  addMotionSensorServiceJson["service"] = "MotionSensor";
  //  String addMotionSensorServiceJsonString;
  //
  //  serializeJson(addMotionSensorServiceJson, addMotionSensorServiceJsonString);
  //  Serial.println(addMotionSensorServiceJsonString.c_str());
  //  if (client.publish(servicetopic, addMotionSensorServiceJsonString.c_str()))
  //    Serial.println("Motion Service Added");
  StaticJsonDocument<500> addThermostatJson;

  addThermostatJson["name"] = chipIdAC.c_str();
  addThermostatJson["service"] = "Thermostat";
  addThermostatJson["service_name"] = "AC";
  addThermostatJson["CurrentRelativeHumidity"] = measuredHumidity;
  String addThermostatJsonString;
  serializeJson(addThermostatJson, addThermostatJsonString);
  Serial.println(addThermostatJsonString.c_str());
  if (client.publish(addtopic, addThermostatJsonString.c_str()))
    Serial.println("Thermostat Service Added");


  StaticJsonDocument<500> addfanJson;
  addfanJson["name"] = chipIdACFan.c_str();
  addfanJson["service"] = "Fanv2";
  addfanJson["service_name"] = "Fan";
  addfanJson["SwingMode"] = ac_swing_mode;
  addfanJson["RotationSpeed"] = ac_flow / 2.0 * 100;
  String addfanJsonString;

  serializeJson(addfanJson, addfanJsonString);
  Serial.println(addfanJsonString.c_str());

  if (client.publish(addtopic, addfanJsonString.c_str()))
    Serial.println("fan Service Added");



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
  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipId) {
    Json["value"] = lightBulbOn;
  }
  else if (accessoryCharacteristic == std::string("Brightness") && String(accessoryName) == chipId) {
    Json["value"] = lightBulbBrightness;
  }
//  else if (accessoryCharacteristic == std::string("Hue") && String(accessoryName) == chipId) {
//    Json["value"] = lightBulbHue;
//  }
//  else if (accessoryCharacteristic == std::string("Saturation") && String(accessoryName) == chipId) {
//    Json["value"] = lightBulbSaturation;
//  }
  else  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipIdACFan) {

  }
  else if (accessoryCharacteristic == std::string("CurrentTemperature")) {
    Json["value"] = measuredTemperature;
  }
  else if (accessoryCharacteristic == std::string("CurrentRelativeHumidity")) {
    Json["value"] = measuredHumidity;
  }
  //  else if (accessoryCharacteristic == std::string("MotionDetected")) {
  //    Json["value"] = MotionDetected;
  //  }
  else if (accessoryCharacteristic == std::string("TargetHeatingCoolingState")) {
    if (!ac_power_on)
      Json["value"] = 0;
    else if (ac_heat == 0) {
      Json["value"] = 2;
    }
    else {
      Json["value"] = 1;
    }
  }
  else if (accessoryCharacteristic == std::string("TargetTemperature")) {
    Json["value"] = ac_temperature;
  }
  else if (accessoryCharacteristic == std::string("SwingMode")) {
    //Json["value"] = Ac_Change_Air_Swing;
  }
  else if (accessoryCharacteristic == std::string("RotationSpeed")) {
    Json["value"] = ac_flow / 2.0 * 100;
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
    if (String(accessoryName) == chipId || String(accessoryName) == chipIdAC || String(accessoryName) == chipIdACFan) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];

      Serial.print("Set -> ");
      Serial.print(accessoryCharacteristic);
      Serial.print(" to ");
      Serial.println((int)mqttAccessory["value"]);

      if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipId) {
        lightBulbOn = (bool) mqttAccessory["value"];
        setRGBW();
      }
      else if (accessoryCharacteristic == std::string("Brightness") && String(accessoryName) == chipId) {
        lightBulbBrightness = (int) mqttAccessory["value"];
        setRGBW();
      }
//      else if (accessoryCharacteristic == std::string("Hue") && String(accessoryName) == chipId) {
//        lightBulbHue = (int) mqttAccessory["value"];
//        setRGBW();
//      }
//      else if (accessoryCharacteristic == std::string("Saturation") && String(accessoryName) == chipId) {
//        lightBulbSaturation = (int) mqttAccessory["value"];
//        setRGBW();
//      }
      else  if (accessoryCharacteristic == std::string("On") && String(accessoryName) == chipIdACFan) {

      }
      else if (accessoryCharacteristic == std::string("TargetHeatingCoolingState")) {

        switch ((int) mqttAccessory["value"]) {
          case 0:        // turnOff
            Ac_Power_Down();           
            break;
          case 1:        // Heat
            Ac_Activate(ac_temperature, ac_flow, 1);
              ac_heat=1;
            break;
          case 2:        // Cool
            Ac_Activate(ac_temperature, ac_flow, 0);
              ac_heat=0;
            break;
           case 3:        // AUTO
              ac_heat= (ac_temperature>measuredTemperature)?1:0;
              Ac_Activate(ac_temperature, ac_flow, ac_heat);
             
            break;
         
        };
      }
      else if (accessoryCharacteristic == std::string("TargetTemperature")) {
        ac_temperature = (int) mqttAccessory["value"];
        if ( ac_temperature < 18)
          ac_temperature = 18;

        if (ac_temperature > 30)
          ac_temperature = 18;
        Ac_Activate(ac_temperature, ac_flow, ac_heat);
      }
      else if (accessoryCharacteristic == std::string("TemperatureDisplayUnits")) {

      }
      else if (accessoryCharacteristic == std::string("SwingMode")) {
        Ac_Change_Air_Swing((int) mqttAccessory["value"]);
      }
      else if (accessoryCharacteristic == std::string("RotationSpeed")) {
        Ac_Activate(ac_temperature, abs(((int) mqttAccessory["value"]) * 2.0 / 100), ac_heat);
      }

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
  Serial.print("sent AC Swing: ");
  Serial.println(ac_code_to_sent);
}

void Ac_Power_Down() {
  ac_code_to_sent = 0x88C0051;
  Ac_Send_Code(ac_code_to_sent);
  ac_power_on = 0;
  Serial.println("sent AC off");

}

void Ac_Air_Clean(int air_clean) {
  if (air_clean == '1')
    ac_code_to_sent = 0x88C000C;
  else
    ac_code_to_sent = 0x88C0084;
  Ac_Send_Code(ac_code_to_sent);
  ac_air_clean_state = air_clean;
}



void hsi2rgbw(float H, float S, float I, int* rgbw) {
  int r, g, b, w;
  float cos_h, cos_1047_h;
  H = fmod(H, 360); // cycle H around to 0-360 degrees
  H = 3.14159 * H / (float)180; // Convert to radians.
  S = S / 100;
  S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
  I = I / 100;
  I = I > 0 ? (I < 1 ? I : 1) : 0;

  if (H < 2.09439) {
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667 - H);
    r = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
    g = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
    b = 0;
    w = 255 * (1 - S) * I;
  } else if (H < 4.188787) {
    H = H - 2.09439;
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667 - H);
    g = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
    b = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
    r = 0;
    w = 255 * (1 - S) * I;
  } else {
    H = H - 4.188787;
    cos_h = cos(H);
    cos_1047_h = cos(1.047196667 - H);
    b = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
    r = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
    g = 0;
    w = 255 * (1 - S) * I;
  }
  rgbw[0] = r;
  rgbw[1] = g;
  rgbw[2] = b;
  rgbw[3] = w;
}

void setRGBW() {
  int LedPins[] = {0, 0, 0, 0};
  if (lightBulbOn == true)
    LedPins[3]= map(lightBulbBrightness,0,100,0,255);
    //hsi2rgbw(lightBulbHue, lightBulbSaturation, lightBulbBrightness, LedPins);

  Serial.print("Setting LEDs: {");
//  Serial.print("r: ");
//  Serial.print( LedPins[0]);
//  Serial.print(" , g: ");
//  Serial.print( LedPins[1]);
//  Serial.print(" , b: ");
//  Serial.print( LedPins[2]);
  Serial.print(" , w: ");
  Serial.print( LedPins[3]);
  Serial.println("}");

//  analogWrite(RED_LedPin, LedPins[0]);
//  analogWrite(GREEN_LedPin, LedPins[1]);
//  analogWrite(BLUE_LedPin, LedPins[2]);
  analogWrite(WHITE_LedPin, LedPins[3]);

}
