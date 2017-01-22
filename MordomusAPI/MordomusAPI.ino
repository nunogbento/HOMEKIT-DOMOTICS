
#include <ESP8266FtpServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char* host = "MordomusServer";
const char* ssid = "MEO-AADB7D";
const char* password = "49809012A8";
const byte DeviceId = 1;

const byte preamble=0xAA;
const byte oncmd=0x64;
const byte offcmd=0x01;

ESP8266WebServer webSrv(80);
FtpServer ftpSrv;   



void returnOK() {
  webSrv.send(200, "text/plain", "");
}

void returnFail(String msg) {
  webSrv.send(500, "text/plain", msg + "\r\n");
}

bool loadSPIFFS(String path){
  String dataType = "text/plain";  
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SPIFFS.open(path.c_str(), "r");
 
  if (!dataFile)
    return false;
 
  if (webSrv.streamFile(dataFile, dataType) != dataFile.size()) {    
  }

  dataFile.close();
  return true;
}

void handleNotFound(){
   if(loadSPIFFS(webSrv.uri())) return;
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webSrv.uri();
  message += "\nMethod: ";
  message += (webSrv.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += webSrv.args();
  message += "\n";
  for (uint8_t i=0; i<webSrv.args(); i++){
    message += " " + webSrv.argName(i) + ": " + webSrv.arg(i) + "\n";
  }
  webSrv.send(404, "text/plain", message);
}

void handleTurnOutputOn(){
 if(webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");  
 byte address=(byte)webSrv.arg("address").toInt();
 
 Serial.write(preamble);
 delay(1);
 Serial.write(address);
 delay(1);
 Serial.write(oncmd);
 webSrv.send(200, "text/plain", "");
}

void handleTurnOutputOff(){
 if(webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");  
 byte address=(byte)webSrv.arg("address").toInt();
 
 Serial.write(preamble); 
 delay(1);
 Serial.write(address) ;
 delay(1);
 Serial.write(offcmd) ;
 webSrv.send(200, "text/plain", "");
}




void setup() {
  Serial.begin(14400);  
  WiFi.begin(ssid, password);
 
  // Wait for connection
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds   
    delay(2000);
  }
  
  if(i == 21){
   while(1) delay(500);
  }
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);    
  }
  
  if (SPIFFS.begin()) {
        ftpSrv.begin("esp8266","esp8266");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
  }

  //SETUP HTTP
  webSrv.on("/turnoutputon", HTTP_POST, handleTurnOutputOn);
  webSrv.on("/turnoutputoff", HTTP_POST, handleTurnOutputOff);
  webSrv.onNotFound(handleNotFound);
  //Http Server Start
  webSrv.begin();
  
 
}


void loop() {
    ftpSrv.handleFTP();       
    webSrv.handleClient();    
}

