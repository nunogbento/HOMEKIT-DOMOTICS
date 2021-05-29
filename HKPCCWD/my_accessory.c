#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "configuration.h"

void my_accessory_identify(homekit_value_t _value) {
  printf("accessory identify\n");
}

homekit_characteristic_t cha_on_0 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_1 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_2 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_3 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_4 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_5 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_6 = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_on_7 = HOMEKIT_CHARACTERISTIC_(ON, false);


homekit_characteristic_t cha_programmable_switch_event_0 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_1 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_2 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_3 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_4 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_5 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_6 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_7 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_8 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t cha_programmable_switch_event_9 = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);




homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "NB"),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
      HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_0,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 0"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_1,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 1"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_2,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 2"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_3,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 3"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_4,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 4"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_5,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 5"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_6,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 6"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_7,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 7"),
      NULL
    }),
    NULL
  }),
  /*HOMEKIT_ACCESSORY(.id = 2, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulbs 1"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_3,
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 3"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_4,
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 4"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_5,
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 5"),
      NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 3, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
                              HOMEKIT_CHARACTERISTIC(NAME, "lightbulbs 2"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_6,
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 6"),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_7,
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 7"),
      NULL
    }),
    NULL
  }),
  /*HOMEKIT_ACCESSORY(.id = 4, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics =(homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 3"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_3,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 3"),
      NULL
    }),
    NULL
    }),
    HOMEKIT_ACCESSORY(.id = 5, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]){
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 4"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_4,
     // HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 4"),
      NULL
    }),
    NULL
    }),
    HOMEKIT_ACCESSORY(.id = 6, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]){
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 5"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_5,
     // HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 5"),
      NULL
    }),
    NULL
    }),
    HOMEKIT_ACCESSORY(.id = 7, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]){
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 6"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_6,
     // HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 6"),
      NULL
    }),
    NULL
    }),
    HOMEKIT_ACCESSORY(.id = 8, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics =(homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 7"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on_7,
      //HOMEKIT_CHARACTERISTIC(NAME, "lightbulb 7"),
      NULL
    }),
    NULL
    }),*/
  //programable switches
  HOMEKIT_ACCESSORY(.id = 9, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 0"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 0"),
                             &cha_programmable_switch_event_0,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 1"),
                             &cha_programmable_switch_event_1,
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 2"),
                             &cha_programmable_switch_event_2,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 3"),
                             &cha_programmable_switch_event_3,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 4"),
                             &cha_programmable_switch_event_4,
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 5"),
                             &cha_programmable_switch_event_5,
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 6"),
                             &cha_programmable_switch_event_6,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 7"),
                             &cha_programmable_switch_event_7,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
     // HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 8"),
                             &cha_programmable_switch_event_8,
                             NULL
    }),
     HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      //HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 9"),
                             &cha_programmable_switch_event_9,
                             NULL
    }),
    NULL
  }),
 /* HOMEKIT_ACCESSORY(.id = 10, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 1"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
    
                             &cha_programmable_switch_event_1,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 11, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 2"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      
                             &cha_programmable_switch_event_2,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 12, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 3"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
    
                             &cha_programmable_switch_event_3,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 13, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 4"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      
                             &cha_programmable_switch_event_4,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 14, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 5"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      
                             &cha_programmable_switch_event_5,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 15, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 6"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
     
                             &cha_programmable_switch_event_6,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 16, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 7"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
     
                             &cha_programmable_switch_event_7,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 17, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 8"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      
                             &cha_programmable_switch_event_8,
                             NULL
    }),
    NULL
  }),
  HOMEKIT_ACCESSORY(.id = 18, .category = homekit_accessory_category_switch, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Stateless Programmable Switch 9"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      
                             &cha_programmable_switch_event_9,
                             NULL
    }),
    NULL
  }),*/
  NULL
};




homekit_server_config_t accessory_config = {
  .accessories = accessories,
  .password = "111-11-111"
};
