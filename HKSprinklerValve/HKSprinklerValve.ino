#include <Arduino.h>
#include "ValveController.h"


#include <ESP8266WiFi.h>

#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <arduino_homekit_server.h>
#include "configuration.h"
#include "ValveController.h"


String chipId;
const char* mqtt_server = "192.168.1.109";
const char* mqttuser = "";
const char* mqttpass = "";

const char* logTopic = "irrigation/update/valve";
const char* intopic = "irrigation/set/valve";


ValveController VC1(VALVE_1_PIN, DEFAULT_DURATION);
ValveController VC2(VALVE_2_PIN, DEFAULT_DURATION);
ValveController VC3(VALVE_3_PIN, DEFAULT_DURATION);
ValveController VC4(VALVE_4_PIN, DEFAULT_DURATION);


WiFiClient wclient;
PubSubClient pubSubClient(wclient);


void pubSubcallback(char* topic, byte* payload, unsigned int length) {
}

void setup() {
  Serial.begin(115200);
  chipId = ACCESSORY_NAME + String(ESP.getChipId());
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.setConfigPortalTimeout(240);  // auto close configportal after n seconds
  if (!wifiManager.autoConnect(chipId.c_str())) {
    Serial.println(F("Failed to connect. Reset and try again..."));
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  pubSubClient.setServer(mqtt_server, 1883);
  pubSubClient.connect(chipId.c_str(), mqttuser, mqttpass);
  pubSubClient.setCallback(pubSubcallback);
  pubSubClient.subscribe(intopic);

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
  VC1.Loop();
  VC2.Loop();
  VC3.Loop();
  VC4.Loop();
}

//==============================
// HomeKit setup and loop
//==============================


// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_active_1;
extern "C" homekit_characteristic_t cha_active_2;
extern "C" homekit_characteristic_t cha_active_3;
extern "C" homekit_characteristic_t cha_active_4;

extern "C" homekit_characteristic_t cha_in_use_1;
extern "C" homekit_characteristic_t cha_in_use_2;
extern "C" homekit_characteristic_t cha_in_use_3;
extern "C" homekit_characteristic_t cha_in_use_4;

extern "C" homekit_characteristic_t cha_valve_type_1;
extern "C" homekit_characteristic_t cha_valve_type_2;
extern "C" homekit_characteristic_t cha_valve_type_3;
extern "C" homekit_characteristic_t cha_valve_type_4;

extern "C" homekit_characteristic_t cha_valve_s_d_1;
extern "C" homekit_characteristic_t cha_valve_s_d_2;
extern "C" homekit_characteristic_t cha_valve_s_d_3;
extern "C" homekit_characteristic_t cha_valve_s_d_4;

extern "C" homekit_characteristic_t cha_valve_r_d_1;
extern "C" homekit_characteristic_t cha_valve_r_d_2;
extern "C" homekit_characteristic_t cha_valve_r_d_3;
extern "C" homekit_characteristic_t cha_valve_r_d_4;

static uint32_t next_heap_millis = 0;

void my_homekit_setup() {
  cha_active_1.setter = set_active_1;
  cha_active_2.setter = set_active_2;
  cha_active_3.setter = set_active_3;
  cha_active_4.setter = set_active_4;

  cha_valve_s_d_1.setter = set_d_1;
  cha_valve_s_d_2.setter = set_d_2;
  cha_valve_s_d_3.setter = set_d_3;
  cha_valve_s_d_4.setter = set_d_4;

  arduino_homekit_setup(&accessory_config);


  VC1.setActiveChangeCallback([&](u_int8_t active) {
    cha_active_1.value.uint8_value = active;
    homekit_characteristic_notify(&cha_active_1, cha_active_1.value);
  });

  VC1.setRemainingDurationChangeCallback([&](u_int16_t duration) {
    cha_valve_r_d_1.value.uint16_value = duration;
    homekit_characteristic_notify(&cha_valve_r_d_1, cha_valve_r_d_1.value);
  });


  VC2.setActiveChangeCallback([&](u_int8_t active) {
    cha_active_2.value.uint8_value = active;
    homekit_characteristic_notify(&cha_active_2, cha_active_2.value);
  });

  VC2.setRemainingDurationChangeCallback([&](u_int16_t duration) {
    cha_valve_r_d_2.value.uint16_value = duration;
    homekit_characteristic_notify(&cha_valve_r_d_2, cha_valve_r_d_2.value);
  });


  VC3.setActiveChangeCallback([&](u_int8_t active) {
    cha_active_3.value.uint8_value = active;
    homekit_characteristic_notify(&cha_active_3, cha_active_3.value);
  });

  VC3.setRemainingDurationChangeCallback([&](u_int16_t duration) {
    cha_valve_r_d_3.value.uint16_value = duration;
    homekit_characteristic_notify(&cha_valve_r_d_3, cha_valve_r_d_3.value);
  });


  VC4.setActiveChangeCallback([&](u_int8_t active) {
    cha_active_4.value.uint8_value = active;
    homekit_characteristic_notify(&cha_active_4, cha_active_4.value);
  });

  VC4.setRemainingDurationChangeCallback([&](u_int16_t duration) {
    cha_valve_r_d_4.value.uint16_value = duration;
    homekit_characteristic_notify(&cha_valve_r_d_4, cha_valve_r_d_4.value);
  });
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
}

void set_active_1(const homekit_value_t v) {
  uint8_t active = v.uint8_value;
  cha_active_1.value.uint8_value = active;  //sync the value
  if (active) {
    VC1.TurnOn();
    LOG_D("VALVE 1 active");
  } else {
    VC1.TurnOff();
    LOG_D("VALVE 1 inactive");
  }
  cha_in_use_1.value.uint8_value = active;
  homekit_characteristic_notify(&cha_in_use_1, cha_in_use_1.value);
}

void set_d_1(const homekit_value_t v) {
  uint16_t duration = v.uint16_value;
  cha_valve_s_d_1.value.uint16_value = duration;  //sync the value
  VC1.SetDuration(duration);
}


void set_active_2(const homekit_value_t v) {
  uint8_t active = v.uint8_value;
  cha_active_2.value.uint8_value = active;  //sync the value
  if (active) {
    VC2.TurnOn();
    LOG_D("VALVE 2 active");
  } else {
    VC2.TurnOff();
    LOG_D("VALVE 2 inactive");
  }
  cha_in_use_2.value.uint8_value = active;
  homekit_characteristic_notify(&cha_in_use_2, cha_in_use_2.value);
}

void set_d_2(const homekit_value_t v) {
  uint16_t duration = v.uint16_value;
  cha_valve_s_d_2.value.uint16_value = duration;  //sync the value
  VC2.SetDuration(duration);
}

void set_active_3(const homekit_value_t v) {
  uint8_t active = v.uint8_value;
  cha_active_3.value.uint8_value = active;  //sync the value
  if (active) {
    VC3.TurnOn();
    LOG_D("VALVE 3 active");
  } else {
    VC3.TurnOff();
    LOG_D("VALVE 3 inactive");
  }
  cha_in_use_3.value.uint8_value = active;
  homekit_characteristic_notify(&cha_in_use_3, cha_in_use_3.value);
}
void set_d_3(const homekit_value_t v) {
  uint16_t duration = v.uint16_value;
  cha_valve_s_d_3.value.uint16_value = duration;  //sync the value
  VC3.SetDuration(duration);
}

void set_active_4(const homekit_value_t v) {
  uint8_t active = v.uint8_value;
  cha_active_4.value.uint8_value = active;  //sync the value
  if (active) {
    VC4.TurnOn();
    LOG_D("VALVE 4 active");
  } else {
    VC4.TurnOff();
    LOG_D("VALVE 4 inactive");
  }
  cha_in_use_4.value.uint8_value = active;
  homekit_characteristic_notify(&cha_in_use_4, cha_in_use_4.value);
}


void set_d_4(const homekit_value_t v) {
  uint16_t duration = v.uint16_value;
  cha_valve_s_d_4.value.uint16_value = duration;  //sync the value
  VC4.SetDuration(duration);
}


void postToLog(float t, float h) {
  StaticJsonDocument<800> json;
  json["name"] = chipId.c_str();
  json["temperature"] = t;
  json["humidity"] = h;
  String jsonString;
  serializeJson(json, jsonString);
  if (pubSubClient.publish(logTopic, jsonString.c_str()))
    LOG_D("Env data posted");
}
