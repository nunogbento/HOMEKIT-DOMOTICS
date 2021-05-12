#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "wifi_info.h"

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

bool is_on = false;
float current_brightness =  100.0;

bool is_onB = false;
float current_brightnessB =  100.0;

void setup() {
	Serial.begin(115200);
	wifi_connect(); // in wifi_info.h
	//homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
	my_homekit_setup();
}

void loop() {
	my_homekit_loop();
	delay(10);
}

//==============================
// HomeKit setup and loop
//==============================
#define L1PIN       13
#define L2PIN       14

// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_bright;

extern "C" homekit_characteristic_t cha_onB;
extern "C" homekit_characteristic_t cha_brightB;


static uint32_t next_heap_millis = 0;

void my_homekit_setup() {

  cha_on.setter = set_on;
  cha_bright.setter = set_bright;

  cha_onB.setter = set_onB;
  cha_brightB.setter = set_brightB;
 
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
}

void set_on(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on.value.bool_value = on; //sync the value

    if(on) {
        is_on = true;
        Serial.println("On");
    } else  {
        is_on = false;
        Serial.println("Off");
    }

    updateBrightness();
}

void set_onB(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_onB.value.bool_value = on; //sync the value

    if(on) {
        is_onB = true;
        Serial.println("On");
    } else  {
        is_onB = false;
        Serial.println("Off");
    }

    updateBrightness();
}


void set_bright(const homekit_value_t v) {
    Serial.println("set_bright");
    int bright = v.int_value;
    cha_bright.value.int_value = bright; //sync the value
    current_brightness = bright;
    updateBrightness();
}

void set_brightB(const homekit_value_t v) {
    Serial.println("set_bright");
    int bright = v.int_value;
    cha_brightB.value.int_value = bright; //sync the value
    current_brightnessB = bright;
    updateBrightness();
}

void updateBrightness(){
  if(!is_on) //lamp - switch to off
    digitalWrite(L1PIN,0);
  else
    analogWrite(L1PIN,map(current_brightness,0,100,0,255));
    
  if(!is_onB) //lamp - switch to off
    digitalWrite(L2PIN,0);
  else
    analogWrite(L2PIN,map(current_brightnessB,0,100,0,255));
}
