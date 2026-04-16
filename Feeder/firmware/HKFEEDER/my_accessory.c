#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "configuration.h"

void my_accessory_identify(homekit_value_t _value) {
  printf("accessory identify\n");
}

homekit_characteristic_t cha_on = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, "Feeder Switch");


homekit_characteristic_t cha_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);

// format: float; min 0, max 100, step 1
homekit_characteristic_t cha_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);




homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, ACCESSORY_NAME),
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "nb"),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
      HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
    HOMEKIT_SERVICE(SWITCH, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on
      , &cha_name
      , NULL
    })
    , HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Temperature"),
                             &cha_temperature,
                             NULL
    })
    , HOMEKIT_SERVICE(HUMIDITY_SENSOR, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Humidity"),
                             &cha_humidity,
                             NULL
    })
    , NULL
  })
  , NULL
};


homekit_server_config_t accessory_config = {
  .accessories = accessories,
  .password = "111-11-111"
};
