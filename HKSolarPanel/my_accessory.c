#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "configuration.h"

void my_accessory_identify(homekit_value_t _value) {
  printf("accessory identify\n");
}

homekit_characteristic_t cha_name_s = HOMEKIT_CHARACTERISTIC_(NAME, "WaterHeaterS");
homekit_characteristic_t cha_name_e = HOMEKIT_CHARACTERISTIC_(NAME, "WaterHeaterE");

// format: bool; min 0, max 1
homekit_characteristic_t cha_active_s = HOMEKIT_CHARACTERISTIC_(ACTIVE, 1);
homekit_characteristic_t cha_active_e = HOMEKIT_CHARACTERISTIC_(ACTIVE, 1);

// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_c_temperature_s = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t cha_c_temperature_e = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);


// format: u_int; min 0, max 3, step 1,
homekit_characteristic_t cha_current_state_s = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 1);
// format: u_int; min 0, max 3, step 1,
homekit_characteristic_t cha_current_state_e = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 1);
// format: u_int; min 0, max 2, step 1,
homekit_characteristic_t cha_target_state_s = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 0);
// format: u_int; min 0, max 2, step 1,
homekit_characteristic_t cha_target_state_e = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 0);


homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_heater, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "nb"),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
      HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(HEATER_COOLER, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_name_s,
      &cha_active_s,
      &cha_c_temperature_s,
      &cha_current_state_s,
      &cha_target_state_s,
      NULL
    }),
    HOMEKIT_SERVICE(HEATER_COOLER, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_name_e,
      &cha_active_e,
      &cha_c_temperature_e,
      &cha_current_state_e,
      &cha_target_state_e,
      NULL
    }),
    NULL
  }),
  NULL
};


homekit_server_config_t accessory_config = {
  .accessories = accessories,
  .password = "111-11-111"
};
