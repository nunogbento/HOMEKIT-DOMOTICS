#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "configuration.h"

void my_accessory_identify(homekit_value_t _value) {
  printf("accessory identify\n");
}

homekit_characteristic_t cha_active_1 = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t cha_in_use_1 = HOMEKIT_CHARACTERISTIC_(IN_USE, 0);
homekit_characteristic_t cha_valve_type_1 = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, VALVE_1_TYPE);
//homekit_characteristic_t cha_name_1 = HOMEKIT_CHARACTERISTIC_(NAME, "valve 1");

homekit_characteristic_t cha_active_2 = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t cha_in_use_2 = HOMEKIT_CHARACTERISTIC_(IN_USE, 0);
homekit_characteristic_t cha_valve_type_2 = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, VALVE_2_TYPE);
//homekit_characteristic_t cha_name_2 = HOMEKIT_CHARACTERISTIC_(NAME, "valve 2");

homekit_characteristic_t cha_active_3 = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t cha_in_use_3 = HOMEKIT_CHARACTERISTIC_(IN_USE, 0);
homekit_characteristic_t cha_valve_type_3 = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, VALVE_3_TYPE);
//homekit_characteristic_t cha_name_3 = HOMEKIT_CHARACTERISTIC_(NAME, "valve 3");

homekit_characteristic_t cha_active_4 = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t cha_in_use_4 = HOMEKIT_CHARACTERISTIC_(IN_USE, 0);
homekit_characteristic_t cha_valve_type_4 = HOMEKIT_CHARACTERISTIC_(VALVE_TYPE, VALVE_4_TYPE);
//homekit_characteristic_t cha_name_4 = HOMEKIT_CHARACTERISTIC_(NAME, "valve 4");


homekit_accessory_t *accessories[] = {
   HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_bridge, .services=(homekit_service_t*[]) {
    	// HAP section 8.17:
    	// For a bridge accessory, only the primary HAP accessory object must contain this(INFORMATION) service.
    	// But in my test,
    	// the bridged accessories must contain an INFORMATION service,
    	// otherwise the HomeKit will reject to pair.
    	HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "SPINKLERS"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "nb"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
        NULL
    }),
  HOMEKIT_ACCESSORY(.id = 2, .category = homekit_accessory_category_sprinkler, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "SPINKLERS_1"),      
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(VALVE, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_active_1
      ,&cha_in_use_1
      ,&cha_valve_type_1
      , NULL
    }),
     NULL
    }
    ),
    HOMEKIT_ACCESSORY(.id = 3, .category = homekit_accessory_category_sprinkler, .services = (homekit_service_t*[]) {
      HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "SPINKLERS_2"),      
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
       HOMEKIT_SERVICE(VALVE, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
       &cha_active_2
      ,&cha_in_use_2
      ,&cha_valve_type_2
      ,NULL
    }),
     NULL
  }
  ),
  HOMEKIT_ACCESSORY(.id = 4, .category = homekit_accessory_category_sprinkler, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "SPINKLERS_3"),    
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(VALVE, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
       &cha_active_3
      ,&cha_in_use_3
      ,&cha_valve_type_3
      , NULL
    }),
     NULL
  }
  ),
    HOMEKIT_ACCESSORY(.id = 5, .category = homekit_accessory_category_sprinkler, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "SPINKLERS_4"),      
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(VALVE, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_active_4
      ,&cha_in_use_4
      ,&cha_valve_type_4
      , NULL
    })
    , NULL
  })
  , NULL
};


homekit_server_config_t accessory_config = {
  .accessories = accessories,
  .password = "111-11-111"
};
