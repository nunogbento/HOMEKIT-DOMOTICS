#ifndef TEMPERATUREANDHUMIDITYSERVICE_H_
#define TEMPERATUREANDHUMIDITYSERVICE_H_

#include <Arduino.h>
#include "configuration.h"
#include <Wire.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"


struct TemperatureSensorService : Service::TemperatureSensor {  // A standalone Temperature sensor
  Adafruit_AM2320 *sensor;
  SpanCharacteristic *temp;  // reference to the Current Temperature Characteristic

  TemperatureSensorService()
    : Service::TemperatureSensor(), sensor() {  // constructor() method

    temp = new Characteristic::CurrentTemperature(0);  // instantiate the Current Temperature Characteristic
    temp->setRange(-50, 100);
  }  // end constructor



  void loop() {

    if (temp->timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds
      temp->setVal(sensor->readTemperature());
    }

  }  // loop
};

struct HumiditySensorService : Service::HumiditySensor {  // A standalone Temperature sensor
  Adafruit_AM2320 *sensor;
  SpanCharacteristic *crh;  // reference to the Current Temperature Characteristic

  HumiditySensorService()
    : Service::HumiditySensor(), sensor() {  // constructor() method

    crh=new Characteristic::CurrentRelativeHumidity(0);        // instantiate the Current Temperature Characteristic

    // expand the range from the HAP default of 0-100 to -50 to 100 to allow for negative temperatures

  }  // end constructor



  void loop() {

    if (crh->timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 second
      crh->setVal(sensor->readHumidity());
    }

  }  // loop
};





#endif
