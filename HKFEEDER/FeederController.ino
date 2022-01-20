#include "configuration.h"
#include <Arduino.h>
#include "FeederController.h"


void FeederController::Loop() {


  if (is_on) {
    if (Feeder_Position_Sensor_Aligned) {
      Feeder_Position_Sensor_Aligned = false;
      TurnOff();
      if (callback)
        callback(false);
    }

    unsigned long now = millis();
    if (now - StartFeedOn >= 500)
    {
      TurnOff();
      if (callback)
        callback(false);
    }
  }
}
