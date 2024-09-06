
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
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

#define WHITE_LedPin 16
#define PA_Interrupt_Pin 12
#define PB_Interrupt_Pin 13

#define CONFIG_FILE       "/accessories.conf"
#define LIGHT 1
#define PUSHBTN 2
#define IRSENSOR 3
#define LEAKSENSOR 4
#define VALVE 5
#define SMOKESENSOR 6
#define COSENSOR 7

byte preamble = 0xAA;
byte oncmd = 0x64;
byte offcmd = 0x01;
volatile boolean awakenByInterrupt = false;
volatile boolean PinChangedAnalog = false;
bool isPCCWDConnected = false;

byte pingPccwdBytes[] = {0x57, 0x01, 0x64};

byte initPccwdbytes[] = {
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8b,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8c,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8d,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8e,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x8f,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x90,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x91,
  0xaa, 0x02, 0x8a, 0xaa, 0x02, 0x92,
  0xaa, 0x02, 0xc9, 0xaa, 0x02, 0xc9
};

byte PccwdAccessories[200][2];

const char* mqtt_server = "192.168.1.109";
uint16_t i;
char serviceType[256] = "PCCWDAPI";

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

const unsigned long pingInterval =  3000UL;
const unsigned long IOInterval =  500UL;

static unsigned long lastPingTime = 0 - pingInterval;
static unsigned long lastIOTime = 0 - IOInterval;

RingBufCPP<byte, 200> SerialBuf;
Adafruit_MCP23X17 mcp;
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

byte SerialInput[10];
int SerialInputIndex = 0;

void Log(const char* text) {
  struct AppTrace data ;
  strcpy(data.content, text);
  logger.write(data);

}

SPIFFSLogData<AppTrace> traceData[25];
char chunk[300];

void handleClearLog() {
  char  filename[40];
  time_t today = time(nullptr) / 86400 * 86400; // remove the time part
  struct tm *tinfo = gmtime(&today);
  sprintf_P(filename,
            "%s/%d%02d%02d",
            "/apptrace",
            1900 + tinfo->tm_year,
            tinfo->tm_mon + 1,
            tinfo->tm_mday);

  if (SPIFFS.remove(filename)) {
    sprintf(chunk, "Removed Log File at: %s", filename);
    webSrv.send( 200, "text/html", chunk);
  } else {
    sprintf(chunk, "Failed to Remove Log File at: %s", filename);
    webSrv.send( 400, "text/html", chunk);
  }
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
  for (int i = 0; i < 200; i += 1) {
    if (PccwdAccessories[i][0] > 0) {
      sprintf(chunk, "Address: %d --> Type: %d, data: %d \n", i, PccwdAccessories[i][0], PccwdAccessories[i][1]);
      webSrv.sendContent(chunk);
    }
  }
}


void handleResetAccessories() {
  for (int i = 0; i < 200; i += 1) {
    PccwdAccessories[i][0] = 0;
    PccwdAccessories[i][1] = 0;
  }
  webSrv.send( 200, "text/html", "Accessories Reset");
}


void StoreConfiguration() {
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (configFile) {
    size_t bytes = configFile.write((unsigned char*)PccwdAccessories, 400 ); // C++ way
    configFile.close();
  }
}

void LoadConfiguration() {
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (configFile) {
    char statusBuffer[25];
    Log("Loading configuration:");
    for (int i = 0; i < 200; i += 1) {
      for (int j = 0; j < 2; j++) {
        PccwdAccessories[i][j] = configFile.read();
      }
    }
    configFile.close();
  }
}

void handleAddAccessory() {
  if (webSrv.args() < 2) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();
  byte type =  (address == 0) ? 1 : (byte)webSrv.arg("type").toInt();

  byte data = 0;
  if (webSrv.args() == 3)
    data = (byte)webSrv.arg("data").toInt();

  if (addAccessory(type, address)) {
    PccwdAccessories[address][0] = type;
    PccwdAccessories[address][1] = data;
    StoreConfiguration();
    sprintf(chunk, "Added device type: %d address: %d", type, address);
    Log(chunk);
    webSrv.send(200, "text/plain", chunk);
  } else {
    sprintf(chunk, "Failled to add device type: %d address: %d", type, address);
    Log(chunk);
    webSrv.send(400, "text/plain", chunk);
  }
}

void handleRemoveAccessory() {
  if (webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();

  if (removeAccessory(address)) {
    PccwdAccessories[address][0] = 0;
    PccwdAccessories[address][1] = 0;
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
  if (PccwdAccessories[address][0] != 1)webSrv.send(400, "text/plain", "wrong Accessory Type");
  if (address == 0) {
    PccwdAccessories[address][1] = 100;
    analogWrite(WHITE_LedPin , map(PccwdAccessories[address][1], 0, 100, 0, 255) );
  } else {
    PccwdAccessories[address][1] = 1;
    SerialBuf.add(preamble);
    SerialBuf.add(address);
    SerialBuf.add(oncmd);
  }
  sprintf(chunk, "Turn on device at address: %d", address);
  //Log(chunk);
  webSrv.send(200, "text/plain", "");
}

void handleTurnOff() {
  if (webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");
  byte address = (byte)webSrv.arg("address").toInt();
  if (PccwdAccessories[address][0] != 1)webSrv.send(400, "text/plain", "wrong Accessory Type");
  PccwdAccessories[address][1] = 0;
  if (address == 0) {
    analogWrite(WHITE_LedPin , map(PccwdAccessories[address][1], 0, 100, 0, 255) );
  } else {
    SerialBuf.add(preamble);
    SerialBuf.add(address);
    SerialBuf.add(offcmd);
  }
  sprintf(chunk, "Turn off device at address: %d", address);
  //Log(chunk);
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

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  sprintf(chunk, "handleFileRead:  %s", path.c_str());
  Log(chunk);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type

  if (SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal                                      // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = webSrv.streamFile(file, contentType);    // Send it to the client
    file.close();                                         // Close the file again

    return true;
  }

  sprintf(chunk, "File Not Found:  %s", path.c_str());
  Log(chunk);
  return false;
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = webSrv.upload();
  String filename = upload.filename;
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;


    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                              // Close the file again
      sprintf(chunk, "handleFileUpload Name: %s  size:%d", upload.filename.c_str(), upload.totalSize);
      Log(chunk);
      webSrv.sendHeader("Location", "/success.html");     // Redirect the client to the success page
      webSrv.send(303);
    } else {
      webSrv.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
//{"name"="PCCWDAPI11027766_8","service_name"="Valve 8","service"="Valve","ValveType","default"}
bool addAccessory(byte type, byte address) {
  StaticJsonDocument<800> addAccessoryJson;
  String accessoryId = chipId + "_" + address;
  addAccessoryJson["name"] = accessoryId.c_str();
  if (type == LIGHT) {
    String serviceName = String("Lightbulb ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "Lightbulb";
    if (address == 0)
      addAccessoryJson["Brightness"] = "default";
  }
  else if (type == PUSHBTN) {
    String serviceName = String("Switch ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "StatelessProgrammableSwitch";
  }
  else if (type == IRSENSOR && address <= 16 && address > 0) {
    String serviceName = String("Motion Sensor ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "MotionSensor";
    mcp.pinMode(address - 1, INPUT_PULLUP);
   
  }
  else if (type == LEAKSENSOR && address <= 16 && address > 0) {
    String serviceName = String("Leak Sensor ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "LeakSensor";
    mcp.pinMode(address - 1, INPUT_PULLUP);
   
  }
  else if (type == VALVE && address <= 16 && address > 0) {
    String serviceName = String("Valve ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "Valve";
    addAccessoryJson["ValveType"] = "default";
    mcp.pinMode(address - 1, OUTPUT);
  }
  else if (type == SMOKESENSOR && address <= 16 && address > 0) {
    String serviceName = String("Smoke Sensor ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "SmokeSensor";
    mcp.pinMode(address - 1, INPUT_PULLUP);
   

  } else if (type == COSENSOR && address <= 16 && address > 0) {
    String serviceName = String("CarbonMonoxide Sensor ") + address;
    addAccessoryJson["service_name"] = serviceName;
    addAccessoryJson["service"] = "CarbonMonoxideSensor";
    mcp.pinMode(address - 1, INPUT_PULLUP);
   
  }

  String addAccessoryJsonString;
  serializeJson(addAccessoryJson, addAccessoryJsonString);
  Log((char*)addAccessoryJsonString.c_str());
  return client.publish(addtopic, addAccessoryJsonString.c_str());
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

bool ProgrammableSwitchEvent(const char * accessoryName, int presses) {
  int idx = String(accessoryName).indexOf("_");
  String addressStr = String(accessoryName).substring(idx + 1);


  StaticJsonDocument<200> Json;
  Json["name"] = accessoryName;
  Json["service_name"] = String("Switch ") + addressStr;
  Json["characteristic"] = "ProgrammableSwitchEvent";
  Json["value"] = presses;
  String UpdateJsonString;
  serializeJson(Json, UpdateJsonString);
  //Log((char*)UpdateJsonString.c_str());
  return client.publish(outtopic, UpdateJsonString.c_str());

}


bool getAccessory(const char * accessoryName, const char * accessoryServiceName, const char * accessoryCharacteristic) {
  int idx = String(accessoryName).indexOf("_");
  String addressStr = String(accessoryName).substring(idx + 1);
  byte address = atoi(addressStr.c_str());
  StaticJsonDocument<400> Json;

  Json["name"] = accessoryName;
  Json["service_name"] = accessoryServiceName;
  Json["characteristic"] = accessoryCharacteristic;

  if (accessoryCharacteristic == std::string("On")) {
    Json["value"] = (PccwdAccessories[address][1] > 0);
  }
  else if (accessoryCharacteristic == std::string("Brightness")) {
    Json["value"] = PccwdAccessories[address][1];
  }
  else if (accessoryCharacteristic == std::string("MotionDetected")) {
    Json["value"] = (PccwdAccessories[address][1] == 1);
  }
  else if (accessoryCharacteristic == std::string("LeakDetected")) {
    Json["value"] = (PccwdAccessories[address][1] == 1) ? 0 : 1;
  }
  else if (accessoryCharacteristic == std::string("Active")) {
    Json["value"] = PccwdAccessories[address][1];
  }
  else if (accessoryCharacteristic == std::string("InUse")) {
    Json["value"] = PccwdAccessories[address][1];
  }
  else if (accessoryCharacteristic == std::string("SmokeDetected")) {
    Json["value"] = (PccwdAccessories[address][1] == 1) ? 0 : 1;
  }
  else if (accessoryCharacteristic == std::string("CarbonMonoxideDetected")) {
    Json["value"] = (PccwdAccessories[address][1] == 1) ? 0 : 1;
  }
  else if (accessoryCharacteristic == std::string("FirmwareRevision")) {
    Json["value"] = "0.6.2";
  } 
  else if (accessoryCharacteristic == std::string("ValveType")) {
    Json["value"] = 0;
  }

  String UpdateJsonString;
  serializeJson(Json, UpdateJsonString);
  //Log((char*)UpdateJsonString.c_str());
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

      if (accessoryCharacteristic == std::string("On")) {
        bool accessoryValue = mqttAccessory["value"];
        if (address != 0) {
          PccwdAccessories[address][1] = (accessoryValue) ? 1 : 0 ;
          SerialBuf.add(preamble);
          SerialBuf.add(address);
          SerialBuf.add((accessoryValue) ? oncmd : offcmd);
        } else {
          PccwdAccessories[address][1] = (accessoryValue) ? 100 : 0;
          PinChangedAnalog = true;
        }
      } else if (accessoryCharacteristic == std::string("Brightness")) {
        byte accessoryValue = mqttAccessory["value"];
        PccwdAccessories[address][1] = accessoryValue;
        PinChangedAnalog = true;

      } else if (accessoryCharacteristic == std::string("Active")) {
        byte accessoryValue = mqttAccessory["value"];
        PccwdAccessories[address][1] = accessoryValue;
        mcp.digitalWrite(address - 1, accessoryValue);
        getAccessory(accessoryName, accessoryServiceName, "InUse");
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
  for (int i = 0; i < 16; i += 1) {
    int address = i + 1;
    String accessoryId = chipId + "_" + address;
    String serviceName;

    if (PccwdAccessories[i][0] == LIGHT && i == 0 && PinChangedAnalog) {
      PinChangedAnalog = false;
      analogWrite(WHITE_LedPin , map( PccwdAccessories[i][1], 0, 100, 0, 255) );
    }

    int IOPinValue = mcp.digitalRead(i);
    if (PccwdAccessories[address][0] == IRSENSOR) {
      serviceName = String("Motion Sensor ") + address;
      if (IOPinValue != PccwdAccessories[address][1])
      {
        PccwdAccessories[address][1] = IOPinValue;
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "MotionDetected");
      }
    } else  if (PccwdAccessories[address][0] == SMOKESENSOR) {
      serviceName = String("Smoke Sensor ") + address;
      if (IOPinValue != PccwdAccessories[address][1])
      {
        PccwdAccessories[address][1] = IOPinValue;
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "SmokeDetected");
      }
    } else  if (PccwdAccessories[address][0] == COSENSOR) {
      serviceName = String("CarbonMonoxide Sensor ") + address;
      if (IOPinValue != PccwdAccessories[address][1])
      {
        PccwdAccessories[address][1] = IOPinValue;
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "CarbonMonoxideDetected");
      }
    } else if (PccwdAccessories[address][0] == LEAKSENSOR) {
      serviceName = String("Leak Sensor ") + address;
      if (IOPinValue != PccwdAccessories[address][1])
      {
        PccwdAccessories[address][1] = IOPinValue;
        getAccessory(accessoryId.c_str(), serviceName.c_str(), "LeakDetected");
      }
    }
  }
}

void setup() {
  Serial.begin(14400);

  pinMode(WHITE_LedPin, OUTPUT);
  pinMode(PA_Interrupt_Pin, INPUT);
  pinMode(PB_Interrupt_Pin, INPUT);

  mcp.begin_I2C();

  SPIFFS.begin();

  chipId = String(serviceType) + String(ESP.getChipId());

  StaticJsonDocument<200> jsonReachability;
  jsonReachability["name"] = chipId.c_str();
  jsonReachability["reachable"] = false;
  serializeJson(jsonReachability, jsonReachabilityString);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
   wifiManager.setConfigPortalTimeout(240); // auto close configportal after n seconds
   WiFi.hostname(chipId.c_str());
  if (!wifiManager.autoConnect(chipId.c_str())) 
  {
    Serial.println(F("Failed to connect. Reset and try again..."));
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }
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
   // Serial.printf("Error[%u]: ", error);
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
  webSrv.on("/reset", HTTP_GET, handleResetAccessories);
  webSrv.on("/turnon", HTTP_GET, handleTurnOn);
  webSrv.on("/turnoff", HTTP_GET, handleTurnOff);
  webSrv.on("/log", HTTP_GET, handleLog);
  webSrv.on("/clearlog", HTTP_GET, handleClearLog);
  webSrv.on("/accessories", HTTP_GET, handleAccessories);

  webSrv.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    if (!handleFileRead("/upload.html"))                // send it if it exists
      webSrv.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  webSrv.on("/upload", HTTP_POST,                       // if the client posts to the upload page
  []() {
    webSrv.send(200);
  },                          // Send status 200 (OK) to tell the client we are ready to receive
  handleFileUpload                                    // Receive and save the file
           );
  webSrv.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(webSrv.uri()))                  // send it if it exists
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
        //client.subscribe(mainttopic);
        //addAccessory();
        //setReachability()
        for (int i = 0; i < 200; i += 1) {
          if (PccwdAccessories[i][0] > 0) {
            addAccessory(PccwdAccessories[i][0], i);
          }
        }
        Log("Connected to MQTT Server");
      }
    } else
      client.loop();
  } else {
    ESP.reset();
  }

  while (!SerialBuf.isEmpty()) {
    byte pulled;
    SerialBuf.pull(&pulled);
    Serial.write(pulled);
    delay(2);
  }

  unsigned long now = millis();

  if (!isPCCWDConnected && (now - lastPingTime >= pingInterval)) {
    lastPingTime = now;
    for (int i = 0; i < sizeof pingPccwdBytes; i++) {
      SerialBuf.add(pingPccwdBytes[i]);
    }
  }


  while (Serial.available()) {
    // get the new byte:
    byte inChar = Serial.read();
    // add it to the inputString:
    SerialInput[SerialInputIndex++] = inChar;
    if (inChar == 0x46) {
      SerialInputIndex = 0;
      //parse string and reset
      //sprintf(chunk, "pccwd inputString: %02x,%02x,%02x,%02x", SerialInput[0], SerialInput[1], SerialInput[2], SerialInput[3]);
      //Log(chunk);
      if (SerialInput[0] == 0x49 && SerialInput[1] == 0xaa) {
        if (SerialInput[2] == 0x01 && SerialInput[3] == 0x67) {

          Log("Initializing PCCWD");
          for (int i = 0; i < sizeof pingPccwdBytes; i++) {
            SerialBuf.add(pingPccwdBytes[i]);
          }
          for (int i = 0; i < sizeof initPccwdbytes; i++) {
            SerialBuf.add(initPccwdbytes[i]);
          }
          isPCCWDConnected = true;
          Log("PCCWD initialized");
        } else {
          int address = SerialInput[2];
          int presses = SerialInput[3] - 1;
          //sprintf(chunk, "Switch Event address: %d, Presses:%d", address, presses);
          //Log(chunk);
          String accessoryId = chipId + "_" + address;
          byte bulbAddress = PccwdAccessories[address][1];

          if (bulbAddress > 0 && presses == 0) {
            String bulbId = chipId + "_" + bulbAddress;
            String ServiceName = String("Lightbulb ") + bulbAddress;
            PccwdAccessories[bulbAddress] [1] = (PccwdAccessories[bulbAddress][1] == 0) ? 1 : 0;
            getAccessory(bulbId.c_str(), ServiceName.c_str(), "On");
          }
          ProgrammableSwitchEvent(accessoryId.c_str(), presses);

        }
      }
    }
  }

  if (now - lastIOTime >= IOInterval) {
    lastIOTime = now;
    HadleIO();
    logger.process();
  }


  webSrv.handleClient();

}
