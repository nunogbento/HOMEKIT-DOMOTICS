#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <arduino_homekit_server.h>
#include "configuration.h"
#include "SolarPanelController.h"

String chipId;



SolarPanelController solarPanelController;


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
  solarPanelController.begin(SDA_PIN, SCL_PIN,chipId);

  my_homekit_setup();
}

void loop() {
  ArduinoOTA.handle();
  my_homekit_loop();

}

//==============================
// HomeKit setup and loop
//==============================


// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_c_temperature_s;
extern "C" homekit_characteristic_t cha_c_temperature_e;
extern "C" homekit_characteristic_t cha_t_temperature_s;
extern "C" homekit_characteristic_t cha_t_temperature_e;
extern "C" homekit_characteristic_t cha_current_state_s;
extern "C" homekit_characteristic_t cha_current_state_e;
extern "C" homekit_characteristic_t cha_target_state_s;
extern "C" homekit_characteristic_t cha_target_state_e;






static uint32_t next_heap_millis = 0;

void my_homekit_setup() {

  solarPanelController.setCallback([&](float temperature, Current_State currentState_s, Current_State currentState_e) {
    cha_c_temperature_s.value.float_value = temperature;
    homekit_characteristic_notify(&cha_c_temperature_s, cha_c_temperature_s.value);

    cha_c_temperature_e.value.float_value = temperature;
    homekit_characteristic_notify(&cha_c_temperature_e, cha_c_temperature_e.value);


    //notify current state
    cha_current_state_s.value.uint8_value = (uint8_t)currentState_s;
    homekit_characteristic_notify(&cha_current_state_s, cha_current_state_s.value);
    LOG_D("Notify Current  State:%i", cha_current_state_s.value.uint8_value);

    //notify current state
    cha_current_state_e.value.uint8_value = (uint8_t)currentState_e;
    homekit_characteristic_notify(&cha_current_state_e, cha_current_state_e.value);
    LOG_D("Notify Current  State:%i", cha_current_state_e.value.uint8_value);

  });

  
  cha_target_state_s.setter = set_target_state_s;
  cha_target_state_e.setter = set_target_state_e;
  cha_t_temperature_s.setter = set_target_temperature_s;
  cha_t_temperature_e.setter = set_target_temperature_e;

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

  solarPanelController.Loop();

}


void set_target_state_e(const homekit_value_t v) {
  uint8_t state = v.uint8_value;
  cha_target_state_e.value.uint8_value = state; //sync the value

  LOG_D("target  State:%i", state);
}
void set_target_state_s(const homekit_value_t v) {
  uint8_t state = v.uint8_value;
  cha_target_state_e.value.uint8_value = state; //sync the value

  LOG_D("target  State:%i", state);
}


void set_target_temperature_e(const homekit_value_t v) {
  float htt = v.float_value;
  cha_t_temperature_e.value.float_value = htt; //sync the value
  LOG_D("Heating threshold Temperature set: %f", htt);
  solarPanelController.SetTargetElecticTemperature(htt);
}

void set_target_temperature_s(const homekit_value_t v) {
  float ctt = v.float_value;
  cha_t_temperature_s.value.float_value = ctt; //sync the value
  LOG_D("Cooling threshold Temperature set: %f", ctt);
  solarPanelController.SetTargetSolarTemperature(ctt);
}
