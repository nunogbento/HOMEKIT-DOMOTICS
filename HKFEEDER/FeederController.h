#ifndef FEEDERCONTROLLER_H_
#define FEEDERCONTROLLER_H_

#include "configuration.h"
#include <Servo.h>

volatile bool Feeder_Position_Sensor_Aligned = 0;

IRAM_ATTR void handleInterrupt() {
  Feeder_Position_Sensor_Aligned = 1;
}

typedef std::function<void(bool isOn)> switch_callback;

class FeederController {
    u_int Servo_pin;
    u_int LDR_pin;
    unsigned long StartFeedOn;
    bool is_on = false;
    //init servo library object
    Servo servo;
    switch_callback callback;
  public:

    FeederController(u_int servo_pin, u_int ldr_pin) : servo() {
      Servo_pin = servo_pin;
      pinMode(Servo_pin, OUTPUT);
      LDR_pin = ldr_pin;
      pinMode(LDR_pin, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(LDR_pin), handleInterrupt, FALLING);
       //setup servo motor
      servo.detach();
    }

    void setCallback(switch_callback _callback) {
      callback = _callback;
    }

    void TurnOn() {
      is_on = true;
      StartFeedOn = millis();
      //start moving
      servo.attach(Servo_pin);
      servo.write(77);
    }

    void TurnOff() {
      is_on = false;
      //stop moving
      servo.write(90);
      servo.detach();
    }

    void Loop();
};
#endif
