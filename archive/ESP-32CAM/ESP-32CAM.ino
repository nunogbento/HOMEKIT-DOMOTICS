#include <WiFiManager.h>


#include "src/OV2640.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>


#include "src/SimStreamer.h"
#include "src/OV2640Streamer.h"
#include "src/CRtspSession.h"

#define ENABLE_WEBSERVER
#define ENABLE_RTSPSERVER

OV2640 cam;

#ifdef ENABLE_WEBSERVER
WebServer server(80);
#endif

#ifdef ENABLE_RTSPSERVER
WiFiServer rtspServer(554);
#endif




#ifdef ENABLE_WEBSERVER
void handle_jpg_stream(void)
{
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (1)
  {
    cam.run();
    if (!client.connected())
      break;
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

    client.write((char *)cam.getfb(), cam.getSize());
    server.sendContent("\r\n");
    if (!client.connected())
      break;
  }
}

void handle_jpg(void)
{
  WiFiClient client = server.client();

  cam.run();
  if (!client.connected())
  {
    return;
  }
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-disposition: inline; filename=capture.jpg\r\n";
  response += "Content-type: image/jpeg\r\n\r\n";
  server.sendContent(response);
  client.write((char *)cam.getfb(), cam.getSize());
}

void handleNotFound()
{
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text/plain", message);
}
#endif



CStreamer *streamer;

void setup()
{
  Serial.begin(115200);

  uint64_t mac = ESP.getEfuseMac();
  uint16_t chip = (uint16_t)(mac >> 32);
  
  char chipId[23];
  snprintf(chipId, 23, "COM-%04X%08X", chip, (uint32_t)mac);
  
  //wifi_conn();
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect(chipId);

  
  
  cam.init(esp32cam_aithinker_config);

  

#ifdef ENABLE_WEBSERVER
  server.on("/", HTTP_GET, handle_jpg_stream);
  server.on("/jpg", HTTP_GET, handle_jpg);
  server.onNotFound(handleNotFound);
  server.begin();
#endif

#ifdef ENABLE_RTSPSERVER
  rtspServer.begin();

  //streamer = new SimStreamer(true);             // our streamer for UDP/TCP based RTP transport
  streamer = new OV2640Streamer(cam);             // our streamer for UDP/TCP based RTP transport
#endif
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED) {
  
  } else {
    ESP.restart();
  }

  
#ifdef ENABLE_WEBSERVER
  server.handleClient();
#endif

#ifdef ENABLE_RTSPSERVER
  uint32_t msecPerFrame = 100;
  static uint32_t lastimage = millis();

  // If we have an active client connection, just service that until gone
  streamer->handleRequests(0); // we don't use a timeout here,
  // instead we send only if we have new enough frames
  uint32_t now = millis();
  if (streamer->anySessions()) {
    if (now > lastimage + msecPerFrame || now < lastimage) { // handle clock rollover
      streamer->streamImage(now);
      lastimage = now;

      // check if we are overrunning our max frame rate
      now = millis();
      if (now > lastimage + msecPerFrame) {
        printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
      }
    }
  }

  WiFiClient rtspClient = rtspServer.accept();
  if (rtspClient) {
    Serial.print("client: ");
    Serial.print(rtspClient.remoteIP());
    Serial.println();
    streamer->addSession(rtspClient);
  }
#endif
}
