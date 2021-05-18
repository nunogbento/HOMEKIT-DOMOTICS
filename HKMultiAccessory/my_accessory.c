#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "configuration.h"

void my_accessory_identify(homekit_value_t _value) {
  printf("accessory identify\n");
}

homekit_characteristic_t cha_on = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, "lightbulb A");

#if defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_)
homekit_characteristic_t cha_bright = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 50);
#endif

#if defined(_RGB_) || defined(_RGBW_)
homekit_characteristic_t cha_sat = HOMEKIT_CHARACTERISTIC_(SATURATION, (float) 0);
homekit_characteristic_t cha_hue = HOMEKIT_CHARACTERISTIC_(HUE, (float) 180);
#endif

#if defined(_DUAL_) && !(defined (_RGB_) || defined (_RGBW_))
homekit_characteristic_t cha_onB = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_nameB = HOMEKIT_CHARACTERISTIC_(NAME, "lightbulb B");
#if defined(_DIMMER_)
homekit_characteristic_t cha_brightB = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 50);
#endif
#endif

#if defined(_TH_)&& !defined (_AC_)
// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);

// format: float; min 0, max 100, step 1
homekit_characteristic_t cha_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
#endif

#if defined(_AC_)
// format: bool; min 0, max 1
homekit_characteristic_t cha_active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);

// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);

// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_c_t_temperature = HOMEKIT_CHARACTERISTIC_(COOLING_THRESHOLD_TEMPERATURE, 22);
// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_h_t_temperature = HOMEKIT_CHARACTERISTIC_(HEATING_THRESHOLD_TEMPERATURE, 15);

// format: u_int; min 0, max 3, step 1,
homekit_characteristic_t cha_current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 0);

// format: u_int; min 0, max 2, step 1,
homekit_characteristic_t cha_target_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 0);

// format: float; min 0, max 100, step 1
homekit_characteristic_t cha_rotation_speed = HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 0);

// format: u_int; min 0, max 1, step 1
homekit_characteristic_t cha_swing_mode = HOMEKIT_CHARACTERISTIC_(SWING_MODE, 0);

// format: float; min 0, max 100, step 1
homekit_characteristic_t cha_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
#endif

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
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &cha_on
      , &cha_name
#if defined(_DIMMER_) || defined(_RGB_) || defined(_RGBW_)
      , &cha_bright
#endif
#if defined(_RGB_) || defined(_RGBW_)
      , &cha_sat
      , &cha_hue
#endif
      , NULL
    })
#if defined(_DUAL_) && !(defined(_RGB_) || defined(_RGBW_))
    , HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
        &cha_onB
      , &cha_nameB
#if defined(_DIMMER_)
      , &cha_brightB
#endif
,NULL
    })
#endif
#if defined(_TH_) && !defined(_AC_)
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
#endif
    , NULL
  })
#if defined(_AC_)
  , HOMEKIT_ACCESSORY(.id = 2, .category = homekit_accessory_category_air_conditioner, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "LG AC"),
                             HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                             NULL
    }),
    HOMEKIT_SERVICE(HEATER_COOLER, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "LG AC"),
                             &cha_active
                             , &cha_temperature
                             , &cha_c_t_temperature
                             , &cha_h_t_temperature
                             , &cha_current_state
                             , &cha_target_state
                             , &cha_rotation_speed
                             , &cha_swing_mode
                             ,NULL
    })
    , NULL
  }),
  HOMEKIT_ACCESSORY(.id=3, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
      HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
      NULL
    }),
      HOMEKIT_SERVICE(HUMIDITY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Humidity"),
      &cha_humidity,
      NULL
    }),
    NULL
  })
#endif
  , NULL
};


homekit_server_config_t accessory_config = {
  .accessories = accessories,
  .password = "111-11-111"
};
