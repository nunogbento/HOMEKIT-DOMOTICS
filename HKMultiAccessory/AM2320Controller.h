#ifndef AM2320CONTROLLER_H_
#define AM2320CONTROLLER_H_

#include <Arduino.h>
#include "configuration.h"
#include "AM2320.h"

typedef std::function<void(float temperature, float humidity)> measurement_callback;

class AM2320Controller {
    AM2320 sensor;
    //temp and humidity pooling variables
    unsigned long measureInterval = 2 * 60 * 1000UL;
    unsigned long lastSampleTime = 0 - measureInterval;  // initialize such that a reading is due the first time through loop()

    float currentTemperature = 0;
    float currentHumidity = 0;
    measurement_callback callback;
  public:

    AM2320Controller(): sensor() {}

    void begin(int sda, int scl) {
      sensor.begin(sda, scl);
    }

    void setCallback(measurement_callback _callback) {
      callback = _callback;
    }

    float CurrentTemperature() {
      return currentTemperature;
    }
    float CurrentHumidity() {
      return currentHumidity;
    }

    void Loop() {
      unsigned long now = millis();
      if (now - lastSampleTime >= measureInterval)
      {
        lastSampleTime += measureInterval;
        // sensor.measure() returns boolean value
        // - true indicates measurement is completed and success
        // - false indicates that either sensor is not ready or crc validation failed
        //   use getErrorCode() to check for cause of error.
        if (sensor.measure()) {

          currentTemperature = sensor.getTemperature();
          LOG_D("Temperature: %d", currentTemperature);
          currentHumidity = sensor.getHumidity();
          LOG_D("Humidity: %d", currentHumidity);
          if (callback)
            callback(currentTemperature, currentHumidity);

          else {  // error has occured
            int errorCode = sensor.getErrorCode();
            switch (errorCode) {
              case 1: LOG_D("ERR: Sensor is offline"); break;
              case 2: LOG_D("ERR: CRC validation failed."); break;
            }
          }
        }
      }
    }

};

#endif
