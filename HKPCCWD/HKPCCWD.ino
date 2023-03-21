#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include "configuration.h"
#include <arduino_homekit_server.h>
#include "PCCWDController.h"

String chipId;
PCCWDController pccwdController(Serial);

void setup() {
 
  Serial.begin(14400);
  //Serial.begin(115200);
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
extern "C" homekit_characteristic_t cha_on_0;
extern "C" homekit_characteristic_t cha_on_1;
extern "C" homekit_characteristic_t cha_on_2;
extern "C" homekit_characteristic_t cha_on_3;
extern "C" homekit_characteristic_t cha_on_4;
extern "C" homekit_characteristic_t cha_on_5;
extern "C" homekit_characteristic_t cha_on_6;
extern "C" homekit_characteristic_t cha_on_7;
extern "C" homekit_characteristic_t cha_programmable_switch_event_0;
extern "C" homekit_characteristic_t cha_programmable_switch_event_1;
extern "C" homekit_characteristic_t cha_programmable_switch_event_2;
extern "C" homekit_characteristic_t cha_programmable_switch_event_3;
extern "C" homekit_characteristic_t cha_programmable_switch_event_4;
extern "C" homekit_characteristic_t cha_programmable_switch_event_5;
extern "C" homekit_characteristic_t cha_programmable_switch_event_6;
extern "C" homekit_characteristic_t cha_programmable_switch_event_7;
extern "C" homekit_characteristic_t cha_programmable_switch_event_8;
extern "C" homekit_characteristic_t cha_programmable_switch_event_9;


void my_homekit_setup() {
  cha_on_0.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_0.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS,  on);
  };

  cha_on_1.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_1.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 1,  on);
  };

  cha_on_2.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_2.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 2,  on);
  };

  cha_on_3.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_3.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 3,  on);
  };

  cha_on_4.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_4.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 4,  on);
  };

  cha_on_5.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_5.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 5,  on);
  };

  cha_on_6.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_6.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 6,  on);
  };
  
  cha_on_7.setter = [](const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on_7.value.bool_value = on; //sync the value
    pccwdController.setOnSate(OF8Wd_ADDRESS + 7,  on);
  };

  cha_programmable_switch_event_0.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };

  cha_programmable_switch_event_1.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_2.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_3.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_4.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_5.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_6.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_7.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_8.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };
  cha_programmable_switch_event_9.getter =  []() {
    // Should always return "null" for reading, see HAP section 9.75
    return HOMEKIT_NULL_CPP();
  };

  pccwdController.setProgrammableSwitchEvent_callback([](uint8_t id, PressType pressType) {
  
    uint8_t cha_value = pressType;

    uint8_t index = id - IN10Wd_ADDRESS;

    switch (index) {

      case 0:
        cha_programmable_switch_event_0.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_0, cha_programmable_switch_event_0.value);
        break;
      case 1:
        cha_programmable_switch_event_1.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_1, cha_programmable_switch_event_1.value);
        break;
      case 2:
        cha_programmable_switch_event_2.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_2, cha_programmable_switch_event_2.value);
        break;
      case 3:
        cha_programmable_switch_event_3.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_3, cha_programmable_switch_event_3.value);
        break;
      case 4:
        cha_programmable_switch_event_4.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_4, cha_programmable_switch_event_4.value);
        break;
      case 5:
        cha_programmable_switch_event_5.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_5, cha_programmable_switch_event_5.value);
        break;
      case 6:
        cha_programmable_switch_event_6.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_6, cha_programmable_switch_event_6.value);
        break;
      case 7:
        cha_programmable_switch_event_7.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_7, cha_programmable_switch_event_7.value);
        break;
      case 8:
          cha_programmable_switch_event_8.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_8, cha_programmable_switch_event_8.value);
        break;
      case 9:
        cha_programmable_switch_event_9.value.uint8_value = cha_value;
        homekit_characteristic_notify(&cha_programmable_switch_event_9, cha_programmable_switch_event_9.value);
        break;

    };


  });



  arduino_homekit_setup(&accessory_config);
}



void my_homekit_loop() {
  arduino_homekit_loop();
  pccwdController.loop();
}