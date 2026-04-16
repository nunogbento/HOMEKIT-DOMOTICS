#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <arduino_homekit_server.h>
#include "configuration.h"
#include "FeederController.h"
#include "AM2320Controller.h"

String chipId;
const char* mqtt_server = "192.168.1.109";
const char* mqttuser = "";
const char* mqttpass = "";

const char* logTopic = "sensor/env/update";

FeederController feederController(FEEDERSERVOPIN, FEEDERLDRPIN);
AM2320Controller am2320Controller;

WiFiClient wclient;
PubSubClient pubSubClient(wclient);

void setup() {
  Serial.begin(115200);
  chipId = ACCESSORY_NAME + String(ESP.getChipId());
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.setConfigPortalTimeout(240); // auto close configportal after n seconds
  if (!wifiManager.autoConnect(chipId.c_str()))
  {
    Serial.println(F("Failed to connect. Reset and try again..."));
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  pubSubClient.setServer(mqtt_server, 1883);
  pubSubClient.connect(chipId.c_str(), mqttuser, mqttpass);
  //setup OTA
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(chipId.c_str());
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.onStart([]() {
    LOG_D("OTA Update Start");
  });
  ArduinoOTA.onEnd([]() {
    LOG_D("\nOTA Update End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    LOG_D("OTA Update Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    LOG_D("OTA Update Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) LOG_D("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) LOG_D("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) LOG_D("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) LOG_D("Receive Failed");
    else if (error == OTA_END_ERROR) LOG_D("End Failed");
  });
  ArduinoOTA.begin();
  //homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
  my_homekit_setup();
}

void loop() {
  ArduinoOTA.handle();
  my_homekit_loop();

  if (!pubSubClient.loop()) {
    if (pubSubClient.connect(chipId.c_str(), mqttuser, mqttpass))
      LOG_D("connected to MQTT server");
    else
      LOG_D("Could not connect to MQTT server");
  }

}

//==============================
// HomeKit setup and loop
//==============================


// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_temperature;
extern "C" homekit_characteristic_t cha_humidity;


static uint32_t next_heap_millis = 0;

void my_homekit_setup() {
  cha_on.setter = set_on;

  am2320Controller.setCallback([&](float t, float h) {
    cha_temperature.value.float_value = t;
    homekit_characteristic_notify(&cha_temperature, cha_temperature.value);
    cha_humidity.value.float_value = h;
    homekit_characteristic_notify(&cha_humidity, cha_humidity.value);
    postToLog( t, h);
  });
  am2320Controller.begin(SDA_PIN, SCL_PIN);

  //report the switch value to HomeKit if it is changed (e.g. by a physical button)
  feederController.setCallback([&](bool isOn) {
    cha_on.value.bool_value = isOn;
    homekit_characteristic_notify(&cha_on, cha_on.value);
    if (isOn)LOG_D("Feeder On");
    else LOG_D("Feeder Off");
  });

  arduino_homekit_setup(&accessory_config);
}



void my_homekit_loop() {
  arduino_homekit_loop();
  const uint32_t t = millis();
  if (t > next_heap_millis) {
    // show heap info every 5 seconds
    next_heap_millis = t + 5 * 1000;
    LOG_D("Free heap: %d, HomeKit clients: %d",
          ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
  }

  am2320Controller.Loop();
  feederController.Loop();
}

void set_on(const homekit_value_t v) {
  bool on = v.bool_value;
  cha_on.value.bool_value = on; //sync the value
  if (on) {
    feederController.TurnOn();
    LOG_D("Feeder On");
  } else  {
    feederController.TurnOff();
    LOG_D("Feeder Off");
  }
}

void postToLog(float t, float h) {
  StaticJsonDocument<800> json;
  json["name"] = chipId.c_str();
  json["temperature"] = t;
  json["humidity"] = h;
  String jsonString;
  serializeJson(json, jsonString);
  if (pubSubClient.publish(logTopic, jsonString.c_str()))
    LOG_D("Solar Panel Service Added");
}
