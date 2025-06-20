#ifndef TEMPERATUREANDHUMIDITYSERVICE_H_
#define TEMPERATUREANDHUMIDITYSERVICE_H_

#include <Arduino.h>
#include "configuration.h"

#include "AM2320.h"


struct TemperatureSensorService : Service::TemperatureSensor {  // A standalone Temperature sensor
  AM2320 *sensor;
  SpanCharacteristic *temp;  // reference to the Current Temperature Characteristic

  TemperatureSensorService()
    : Service::TemperatureSensor() {  // constructor() method
    sensor=new AM2320();
    temp = new Characteristic::CurrentTemperature(0);  // instantiate the Current Temperature Characteristic
    temp->setRange(-50, 100);
  }  // end constructor



  void loop() {
    if (temp->timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds
      if (sensor->measure()) {

        temp->setVal(sensor->getTemperature());


      } else {  // error has occured
        int errorCode = sensor->getErrorCode();
        switch (errorCode) {
          case 1: LOG_D("ERR: Sensor is offline"); break;
          case 2: LOG_D("ERR: CRC validation failed."); break;
        }
      }
    }

  }  // loop
};

struct HumiditySensorService : Service::HumiditySensor {  // A standalone Temperature sensor
  AM2320 *sensor;
  SpanCharacteristic *crh;  // reference to the Current Temperature Characteristic

  HumiditySensorService()
    : Service::HumiditySensor() {  // constructor() method
     sensor=new AM2320();
    crh = new Characteristic::CurrentRelativeHumidity(0);  // instantiate the Current Temperature Characteristic

    // expand the range from the HAP default of 0-100 to -50 to 100 to allow for negative temperatures

  }  // end constructor



  void loop() {
    if (crh->timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds
      if (sensor->measure()) {

        crh->setVal(sensor->getHumidity());

      } else {  // error has occured
        int errorCode = sensor->getErrorCode();
        switch (errorCode) {
          case 1: LOG_D("ERR: Sensor is offline"); break;
          case 2: LOG_D("ERR: CRC validation failed."); break;
        }
      }
    }

  }  // loop
};





#endif
