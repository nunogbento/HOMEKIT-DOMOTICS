#include <Arduino.h>
#include "LedController.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <arduino_homekit_server.h>
#include "configuration.h"
#include "LedController.h"
#include "ACController.h"
#include "AM2320Controller.h"

String chipId;

#if !(defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_))
LedController LCA(L1PIN, false);
#if defined(_DUAL_)
LedController LCB(L2PIN, false);
#endif
#elif defined(_DIMMER_) && !(defined(_RGB_) || defined(_RGBW_))
LedController LCA(L1PIN, true);
#if defined(_DUAL_)
LedController LCB(L2PIN, true);
#endif
#elif defined(_RGB_) && ! defined(_RGBW_)
LedController LCA(RED_LedPin, GREEN_LedPin, BLUE_LedPin);
#elif defined(_RGBW_)
LedController LCA(RED_LedPin, GREEN_LedPin, BLUE_LedPin, WHITE_LedPin);
#endif

#if defined(_TH_) && !defined(_AC_)
AM2320Controller am2320Controller;
#endif
#if defined(_AC_)
ACController acController(IR_LED_PIN);
#endif

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
extern "C" homekit_characteristic_t cha_on;

#if defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_)
extern "C" homekit_characteristic_t cha_bright;
#endif

#if defined(_RGB_) || defined(_RGBW_)
extern "C" homekit_characteristic_t cha_sat;
extern "C" homekit_characteristic_t cha_hue;
#endif

#if defined(_DUAL_) && !(defined(_RGB_) || defined(_RGBW_))
extern "C" homekit_characteristic_t cha_onB;
#if defined(_DIMMER_)
extern "C" homekit_characteristic_t cha_brightB;
#endif
#endif

#if defined(_TH_) && !defined(_AC_)
extern "C" homekit_characteristic_t cha_temperature;
extern "C" homekit_characteristic_t cha_humidity;
#endif

#if defined(_AC_)
extern "C" homekit_characteristic_t cha_temperature;
extern "C" homekit_characteristic_t cha_humidity;
extern "C" homekit_characteristic_t cha_active;
extern "C" homekit_characteristic_t cha_current_state;
extern "C" homekit_characteristic_t cha_target_state;
extern "C" homekit_characteristic_t cha_rotation_speed;
extern "C" homekit_characteristic_t cha_swing_mode;
extern "C" homekit_characteristic_t cha_h_t_temperature;
extern "C" homekit_characteristic_t cha_c_t_temperature;

#endif



static uint32_t next_heap_millis = 0;

void my_homekit_setup() {
  cha_on.setter = set_on;
#if defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_)
  cha_bright.setter = set_bright;
#endif
#if defined(_RGB_) || defined(_RGBW_)
  cha_sat.setter = set_sat;
  cha_hue.setter = set_hue;
#endif
#if defined(_DUAL_) && !(defined(_RGB_) || defined(_RGBW_))
  cha_onB.setter = set_onB;
#if defined(_DIMMER_)
  cha_brightB.setter = set_brightB;
#endif
#endif
#if defined(_TH_) && !defined(_AC_)
  am2320Controller.setCallback([&](float t, float h) {
    cha_temperature.value.float_value = t;
    homekit_characteristic_notify(&cha_temperature, cha_temperature.value);
    cha_humidity.value.float_value = h;
    homekit_characteristic_notify(&cha_humidity, cha_humidity.value);
  });
  am2320Controller.begin(SDA_PIN, SCL_PIN);
#endif
#if defined(_AC_)
  acController.setCallback([&](float temperature, float humidity,  CURRENT_C_H_State currentState) {
    cha_temperature.value.float_value = temperature;
    homekit_characteristic_notify(&cha_temperature, cha_temperature.value);

    cha_humidity.value.float_value = humidity;
    homekit_characteristic_notify(&cha_humidity, cha_humidity.value);

  
    //notify current state
    cha_current_state.value.uint8_value = (uint8_t)currentState;
    homekit_characteristic_notify(&cha_current_state, cha_current_state.value);
    LOG_D("Notify Current  State:%i", cha_current_state.value.uint8_value);

  });

  acController.begin(SDA_PIN, SCL_PIN);
  cha_target_state.setter = set_target_state;
  cha_rotation_speed.setter = set_rotation_speed;
  cha_c_t_temperature.setter = set_c_t_temperature;
  cha_h_t_temperature.setter = set_h_t_temperature;
  cha_active.setter = set_active;
  cha_swing_mode.setter = set_swing_mode;
#endif
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
#if defined(_TH_) && !defined(_AC_)
  am2320Controller.Loop();
#endif
#if defined(_AC_)
  acController.Loop();
#endif
}

void set_on(const homekit_value_t v) {
  bool on = v.bool_value;
  cha_on.value.bool_value = on; //sync the value
  if (on) {
    LCA.TurnOn();
    LOG_D("LED A On");
  } else  {
    LCA.TurnOff();
    LOG_D("LED A Off");
  }
}


#if defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_)
void set_bright(const homekit_value_t v) {
  LOG_D("LED A set_bright");
  int bright = v.int_value;
  cha_bright.value.int_value = bright; //sync the value
  LCA.SetBrightness(bright);
}
#endif

#if defined(_RGB_) || defined(_RGBW_)

void set_hue(const homekit_value_t v) {
  LOG_D("sLED A set_hue");
  float hue = v.float_value;
  cha_hue.value.float_value = hue; //sync the value
  LCA.SetHue(hue);
}

void set_sat(const homekit_value_t v) {
  LOG_D("LED A set_sat");
  float sat = v.float_value;
  cha_sat.value.float_value = sat; //sync the value
  LCA.SetSaturation(sat);
}

#endif



#if defined(_DUAL_) && !(defined(_RGB_) || defined(_RGBW_))
void set_onB(const homekit_value_t v) {
  bool on = v.bool_value;
  cha_onB.value.bool_value = on; //sync the value
  if (on) {
    LCB.TurnOn();
    LOG_D("Led B On");
  } else  {
    LCB.TurnOff();
    LOG_D("Led B Off");
  }
}


#if defined(_DIMMER_)
void set_brightB(const homekit_value_t v) {
  LOG_D("LED B set_bright");
  int bright = v.int_value;
  cha_brightB.value.int_value = bright; //sync the value
  LCB.SetBrightness(bright);
}
#endif
#endif


#if defined(_AC_)

void set_target_state(const homekit_value_t v) {
  uint8_t state = v.uint8_value;
  cha_target_state.value.uint8_value = state; //sync the value
  acController.SetTargetState((TARGET_C_H_State)state);
  LOG_D("target  State:%i", state);
}


void set_h_t_temperature(const homekit_value_t v) {
  float htt = v.float_value;
  cha_h_t_temperature.value.float_value = htt; //sync the value
  LOG_D("Heating threshold Temperature set: %f", htt);
  acController.SetHeatingThresholdTemperature(htt);
}

void set_c_t_temperature(const homekit_value_t v) {
  float ctt = v.float_value;
  cha_c_t_temperature.value.float_value = ctt; //sync the value
  LOG_D("Cooling threshold Temperature set: %f", ctt);
  acController.SetCoolingThresholdTemperature(ctt);
}

void set_rotation_speed(const homekit_value_t v) {
  float rotation_speed = v.float_value;
  cha_rotation_speed.value.float_value = rotation_speed; //sync the value
  LOG_D("Rotation Speed In:%f", rotation_speed);
  acController.SetRotationSpeed(rotation_speed);
}

void set_active(const homekit_value_t v){
   bool toActive = v.bool_value;
  cha_active.value.bool_value = toActive; //sync the value
  LOG_D("Active set to:%i", toActive);
  if(toActive)
    acController.SetActive();
  else
    acController.SetInactive();
  
}

void set_swing_mode(const homekit_value_t v) {
  uint8_t swing_mode = v.uint8_value;
  LOG_D("SwingMode:%i", swing_mode);
  cha_swing_mode.value.uint8_value = swing_mode; //sync the value
  if (swing_mode == 1) {
    acController.EnableSwing();
  } else {
    acController.DisableSwing();
  }
}
#endif
