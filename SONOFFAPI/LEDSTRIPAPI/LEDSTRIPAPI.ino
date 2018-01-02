#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char* host = "DimmerServer";
const char* ssid = "MEO-AADB7D";
const char* password = "49809012A8";


const byte LedPin = LED_BUILTIN;

#define WHITE       9// pin for red WHITE
#define RED       9// pin for red LED
#define GREEN    10 // pin for green - never explicitly referenced
#define BLUE     11 // pin for blue - never explicitly referenced
#define SIZE    255
#define DELAY    20
#define HUE_MAX  6.0
#define HUE_DELTA 0.01


ESP8266WebServer webSrv(80);


// Variables will change :
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change :
const long interval = 1000;           // interval at which to blink (milliseconds)


void returnOK() {
  webSrv.send(200, "text/plain", "");
}

void returnFail(String msg) {
  webSrv.send(500, "text/plain", msg + "\r\n");
}



void handleNotFound(){  
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
 //byte address=(byte)webSrv.arg("address").toInt();
 
 webSrv.send(200, "text/plain", "");
}

void handleTurnOutputOff(){
 if(webSrv.args() != 1) return webSrv.send(500, "text/plain", "BAD ARGS");  
 
 //byte address=(byte)webSrv.arg("address").toInt();

 
 webSrv.send(200, "text/plain", "");
}



void handleStatus(){
 byte status=digitalRead(relayPin);
 webSrv.send(200, "text/plain", status?"1":"0");
}


void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Starting");
  WiFi.begin(ssid, password);
 
  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, HIGH);     
     
  // Wait for connection
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds   
    Serial.print ( "." );
    delay(500);
  }
  
  if(i == 21){
   while(1) delay(500);
  }
  
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);    
  }
  
  

  //SETUP HTTP
  webSrv.on("/turnoutputon", HTTP_POST, handleTurnOutputOn);
  webSrv.on("/turnoutputoff", HTTP_POST, handleTurnOutputOff);
  webSrv.on("/turnon", HTTP_GET, handleTurnOutputOn);
  webSrv.on("/status", HTTP_GET, handleStatus);
  webSrv.on("/turnoff", HTTP_GET, handleTurnOutputOff);
  webSrv.onNotFound(handleNotFound);
  //Http Server Start
  webSrv.begin();

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
   // Print the IP address
  Serial.println(WiFi.localIP());
  digitalWrite(LedPin, LOW);     
 
}


void loop() {
  
    webSrv.handleClient();    
    // check to see if it's time to blink the LED; that is, if the
  // difference between the current time and last time you blinked
  // the LED is bigger than the interval at which you want to
  // blink the LED.
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(LedPin, ledState);
  }
}

