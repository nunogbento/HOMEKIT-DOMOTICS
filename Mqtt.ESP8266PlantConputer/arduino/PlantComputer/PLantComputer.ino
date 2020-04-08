

#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <RingBufCPP.h>
#include <RingBufHelpers.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <SPIFFSLogger.h>
#include <time.h>


#define M1_Pin 13
#define M2_Pin 12
#define M3_Pin 14
#define M4_Pin 16

#define CONFIG_FILE       "/accessories.conf"

#define A_S_NOTINUSE 0
#define A_S_AUTO 1
#define A_S_SCH 2
#define A_S_MANUAL 3


#define A_S 0
#define A_D_HT 1
#define A_D_A 2
#define A_D_CH 3



byte PccwdAccessories[4][4];

byte Pump_Pin[] = {M1_Pin, M2_Pin, M3_Pin, M4_Pin};

const char* mqtt_server = "192.168.1.109";
uint16_t i;
char serviceType[256] = "PlantComputer";

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


const unsigned long IOInterval =  5000UL;

static unsigned long lastIOTime = 0 - IOInterval;

Adafruit_ADS1115 ads;

ESP8266WebServer webSrv(80);

WiFiClient wclient;
PubSubClient client(wclient);

File fsUploadFile;

struct AppTrace
{
  char content[256];
};
// create a new logger which will store records in files with names like
// /apptrace/YYYYMMDD, keeping 1 day of history
SPIFFSLogger<AppTrace> logger("/apptrace", 1);

void Log(char* text) {
  struct AppTrace data ;
  strcpy(data.content, text);
  logger.write(data);

}

SPIFFSLogData<AppTrace> traceData[25];
char chunk[300];

void handleClearLog() {
  if (webSrv.args() < 1) return webSrv.send(500, "text/plain", "BAD ARGS");
  String filename = webSrv.arg("address");
  if (SPIFFS.remove(filename))
    webSrv.send( 200, "text/html", filename);
  else
    webSrv.send( 400, "text/html", chunk);
}

void handleLog() {
  const time_t now = time(nullptr);
  const size_t rowCount = logger.rowCount(now);
  size_t total = rowCount > 25 ? 25 : rowCount;
  webSrv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  sprintf(chunk, "Log aT %d: Rows: %d\r\n", ctime(&now), rowCount);
  webSrv.send( 200, "text/html", chunk);
  size_t count = logger.readRows(traceData, now, rowCount - 1 - total, total);
  for (int i = count - 1; i >= 0; i--) {
    sprintf(chunk, "%s - %s \r\n",
            ctime(&traceData[i].timestampUTC),
            traceData[i].data.content);
    webSrv.sendContent(chunk);
  }
}



void handleAccessories() {
  webSrv.setContentLength(CONTENT_LENGTH_UNKNOWN);

  webSrv.send( 200, "text/html", "Accessories: \n");
  for (int i = 0; i < 4; i++) {
    if (PccwdAccessories[i][A_S] > 0) {
      sprintf(chunk, "Address: %d --> Ststus: %d, HT: %d, Active: %d, CH: %d \n", i, PccwdAccessories[i][A_S], PccwdAccessories[i][A_D_HT], PccwdAccessories[i][A_D_A], PccwdAccessories[i][A_D_CH]);
      webSrv.sendContent(chunk);
    }
  }
}


void StoreConfiguration() {
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (configFile) {
    size_t bytes = configFile.write((unsigned char*)PccwdAccessories, 16 ); // C++ way
    configFile.close();
  }
}

void LoadConfiguration() {

  File configFile = SPIFFS.open(CONFIG_FILE, "r");

  if (configFile) {
    char statusBuffer[25];
    Log("Loading configuration:");
    for (int i = 0; i < 4; i += 1) {
      for (int j = 0; j < 4; j++) {
        PccwdAccessories[i][j] = configFile.read();
      }
    }
    configFile.close();
  }

}

void handleAddAccessory() {
  if (webSrv.args() < 2) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();
  byte data = (byte)webSrv.arg("data").toInt();

  if (addAccessory(address)) {
    PccwdAccessories[address][A_S] = A_S_AUTO;
    PccwdAccessories[address][A_D_A] = 0;
    PccwdAccessories[address][A_D_HT] = data;
    StoreConfiguration();
    sprintf(chunk, "Added device address: %d", address);
    Log(chunk);
    webSrv.send(200, "text/plain", chunk);
  } else {
    sprintf(chunk, "Failled to add device address: %d", address);
    Log(chunk);
    webSrv.send(400, "text/plain", chunk);
  }
}

void handleRemoveAccessory() {
  if (webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();

  if (removeAccessory(address)) {
    PccwdAccessories[address][A_S] = A_S_NOTINUSE;
    PccwdAccessories[address][A_D_A] = 0;
    PccwdAccessories[address][A_D_HT] = 0;
    StoreConfiguration();
    sprintf(chunk, "Removed device at address: %d", address);
    Log(chunk);
    webSrv.send(200, "text/plain", chunk);
  } else {
    sprintf(chunk, "Failled to remove device at address: %d", address);
    Log(chunk);
    webSrv.send(400, "text/plain", chunk);
  }
}

void handleTurnOn() {
  if (webSrv.args() != 1) return webSrv.send(400, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();

  String accessoryId = chipId + "_" + address;
  String serviceName = String("IrrigationSystem ") + address;

  if (address >= 0 && address < 4 && PccwdAccessories[address][A_S] > 0) {
    PccwdAccessories[address][A_S] = A_S_MANUAL;
    PccwdAccessories[address][A_D_A] = 1;    
    digitalWrite(Pump_Pin[address], 1);
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "ProgramMode");
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
  }
  sprintf(chunk, "Turn on device at address: %d", address);
  Log(chunk);
  webSrv.send(200, "text/plain", "");
}

void handleAuto() {
  if (webSrv.args() != 1) return webSrv.send(400, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();

  String accessoryId = chipId + "_" + address;
  String serviceName = String("IrrigationSystem ") + address;

  if (address >= 0 && address < 4 && PccwdAccessories[address][A_S] > 0) {
    PccwdAccessories[address][A_S] = A_S_AUTO;
    PccwdAccessories[address][A_D_A] = 0;    
    digitalWrite(Pump_Pin[address], 0);
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "ProgramMode");
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
  }
  sprintf(chunk, "Auto device at address: %d", address);
  Log(chunk);
  webSrv.send(200, "text/plain", "");
}

void handleschedule() {
  if (webSrv.args() != 1) return webSrv.send(400, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();

  String accessoryId = chipId + "_" + address;
  String serviceName = String("IrrigationSystem ") + address;

  if (address >= 0 && address < 4 && PccwdAccessories[address][A_S] > 0) {
    PccwdAccessories[address][A_S] = A_S_SCH;
    PccwdAccessories[address][A_D_A] = 0;    
    digitalWrite(Pump_Pin[address], 0);
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "ProgramMode");
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
  }
  sprintf(chunk, "scheduled device at address: %d", address);
  Log(chunk);
  webSrv.send(200, "text/plain", "");
}


void handleTurnOff() {
  if (webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();
  String accessoryId = chipId + "_" + address;
  String serviceName = String("IrrigationSystem ") + address;
  
  if (address >= 0 && address < 4 && PccwdAccessories[address][A_S] > 0) {
    PccwdAccessories[address][A_S] = A_S_MANUAL;
    PccwdAccessories[address][A_D_A] = 0;
    digitalWrite(Pump_Pin[address], 0);
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "ProgramMode");
    getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
  }
  sprintf(chunk, "Turn off device at address: %d", address);
  Log(chunk);
  webSrv.send(200, "text/plain", "");
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}


bool addAccessory(byte address) {
  StaticJsonDocument<800> addAccessoryJson;
  String accessoryId = chipId + "_" + address;
  addAccessoryJson["name"] = accessoryId.c_str();
  String serviceName = String("IrrigationSystem ") + address;
  addAccessoryJson["service_name"] = serviceName;
  addAccessoryJson["service"] = "IrrigationSystem";
  String addAccessoryJsonString;
  serializeJson(addAccessoryJson, addAccessoryJsonString);
  Log((char*)addAccessoryJsonString.c_str());
  client.publish(addtopic, addAccessoryJsonString.c_str());
  //{"name": "Master Sensor", "service_name": "humidity", "service": "HumiditySensor"}
  StaticJsonDocument<800> addServiceJson;
  addServiceJson["name"] = accessoryId.c_str();
  addServiceJson["service_name"] = "Humidity Sensor";
  addServiceJson["service"] = "HumiditySensor";
  String addServiceJsonString;
  serializeJson(addServiceJson, addServiceJsonString);
  return client.publish(servicetopic, addServiceJsonString.c_str());
}


bool removeAccessory(byte address) {
  StaticJsonDocument<800> removeAccessoryJson;
  String accessoryId = chipId + "_" + address;
  removeAccessoryJson["name"] = accessoryId.c_str();
  String removeAccessoryJsonString;
  serializeJson(removeAccessoryJson, removeAccessoryJsonString);
  Log((char*)removeAccessoryJsonString.c_str());
  return client.publish(removetopic, removeAccessoryJsonString.c_str());
}




bool getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {
  int idx = String(accessoryName).indexOf("_");
  String addressStr = String(accessoryName).substring(idx + 1);
  byte address = atoi(addressStr.c_str());
  StaticJsonDocument<400> Json;

  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;

  if (accessoryCharacteristic == std::string("ProgramMode")) {
    switch (PccwdAccessories[address][0]) {
      case 0:
      case 1:
        Json["value"] = 0;
        break;
      case 2:
        Json["value"] = 1;
        break;
      case 3:
        Json["value"] = 2 ;
        break;
    }
  }
  else if (accessoryCharacteristic == std::string("CurrentRelativeHumidity")) {
    Json["value"] = PccwdAccessories[address][A_D_CH];
  }
  else if (accessoryCharacteristic == std::string("Active")) {
    Json["value"] = PccwdAccessories[address][A_D_A];
  }
  else if (accessoryCharacteristic == std::string("InUse")) {
    Json["value"] = PccwdAccessories[address][A_D_A];
  }

  String UpdateJsonString;
  serializeJson(Json, UpdateJsonString);
  Log((char*)UpdateJsonString.c_str());
  return client.publish(outtopic, UpdateJsonString.c_str());

}

void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  StaticJsonDocument<400> mqttAccessory;

  DeserializationError error =  deserializeJson(mqttAccessory, message);
  if (error)
    return;
  const char* accessoryName = mqttAccessory["name"];
  const char* accessoryServiceName = mqttAccessory["service_name"];

  if (intopic == std::string(topic)) {
    if (String(accessoryName).startsWith(chipId)) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      int idx = String(accessoryName).indexOf("_");
      String addressStr = String(accessoryName).substring(idx + 1);
      byte address = atoi(addressStr.c_str());  

      if (accessoryCharacteristic == std::string("Active")) {
        byte accessoryValue = mqttAccessory["value"];
        PccwdAccessories[address][A_D_A] = accessoryValue;
        PccwdAccessories[address][A_S] = A_S_MANUAL;
         digitalWrite(Pump_Pin[address], accessoryValue);
        //do something to device activate/deactivate pump
        getAccessory(accessoryName, accessoryServiceName, "InUse");      
        getAccessory(accessoryName, accessoryServiceName, "ProgramMode");
      }
    }
  }
  else if (gettopic == std::string(topic)) {
    if (String(accessoryName).startsWith(chipId)) {
      const char* accessoryCharacteristic = mqttAccessory["characteristic"];
      getAccessory(accessoryName, accessoryServiceName, accessoryCharacteristic);
    }
  }
}



void HadleIO() {
  for (int address = 0; address < 4; address++) {
   
    String accessoryId = chipId + "_" + address;
    String serviceName = String("IrrigationSystem ") + address;


    //READ HUMIDITY FROM SENSOR
    int16_t ar = ads.readADC_SingleEnded(address);
    //map reading to percentage
    int CH = map(ar, 25600, 8000, 0, 100);
    PccwdAccessories[address][A_D_CH] = CH;
    getAccessory(accessoryId.c_str(), "Humidity Sensor", "CurrentRelativeHumidity");
    if (PccwdAccessories[address][A_S] == A_S_AUTO) {
      //IF(crosses threshold) activate/deactivate pump
      if ((CH - 5) > PccwdAccessories[address][A_D_HT]){
        PccwdAccessories[address][A_D_A] = 0;
        digitalWrite(Pump_Pin[address], 0);
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
      }
      if ((CH + 5) < PccwdAccessories[address][A_D_HT]){
        PccwdAccessories[address][A_D_A] = 1;
        digitalWrite(Pump_Pin[address], 1);
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "InUse");
      }
    }
  }
}

void setup() {

Serial.begin(115200);

  pinMode(M1_Pin, OUTPUT);
  pinMode(M2_Pin, OUTPUT);
  pinMode(M3_Pin, OUTPUT);
  pinMode(M4_Pin, OUTPUT);

  Wire.begin( 5,4);
  SPIFFS.begin();
  ads.setGain(GAIN_ONE);  

  chipId = String(serviceType) + String(ESP.getChipId());

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
  configTime(0, 0, "pool.ntp.org");
  logger.init();
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(chipId.c_str());
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.onStart([]() {
    Log("Start");
  });
  ArduinoOTA.onEnd([]() {
    Log("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //    sprintf(chunk, "Progress: %u%%", (progress / (total / 100)));
    //    Log(chunk);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Log("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Log("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Log("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Log("Receive Failed");
    else if (error == OTA_END_ERROR) Log("End Failed");
  });
  ArduinoOTA.begin();


  LoadConfiguration();

  client.setServer(mqtt_server, 1883);

  //SETUP HTTP
  webSrv.on("/add", HTTP_GET, handleAddAccessory);
  webSrv.on("/remove", HTTP_GET, handleRemoveAccessory);
  webSrv.on("/auto", HTTP_GET, handleAuto);
  webSrv.on("/schedule", HTTP_GET, handleschedule);
  webSrv.on("/turnon", HTTP_GET, handleTurnOn);
  webSrv.on("/turnoff", HTTP_GET, handleTurnOff);
  webSrv.on("/log", HTTP_GET, handleLog);
  webSrv.on("/clearlog", HTTP_GET, handleClearLog);
  webSrv.on("/accessories", HTTP_GET, handleAccessories); 
  webSrv.onNotFound([]() {                              // If the client requests any URI
      webSrv.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });
  //Http Server Start
  webSrv.begin();
}



void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(chipId.c_str(), mqttuser, mqttpass, reachabilitytopic, 0, false, jsonReachabilityString.c_str())) {
        client.setCallback(callback);
        client.subscribe(intopic);
        client.subscribe(gettopic);        
        //setReachability()
        for (int i = 0; i < 4; i += 1) {
          if (PccwdAccessories[i][0] > 0) {
            addAccessory(i);
          }
        }
        Log("Connected to MQTT Server");
      } else {
        delay(5000);
      }
    }

    if (client.connected())
      client.loop();
  } else {
    ESP.reset();
  }


  unsigned long now = millis();

  if (now - lastIOTime >= IOInterval) {
    lastIOTime = now;
    HadleIO();
  }


  webSrv.handleClient();
  logger.process();
}
