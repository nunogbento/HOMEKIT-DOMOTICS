#ifndef VALVECONTROLLER_H_
#define VALVECONTROLLER_H_

#include "configuration.h"


enum ValveType {
  GENERIC_VALVE = 0,
  IRRIGATION,
  SHOWER_HEAD,
  WATER_FAUCET
};

class ValveController {
  ValveType _type;
  u_int _pin;
  bool _is_active = false;

public:

  ValveController(u_int pin) {
    _pin=pin;

    pinMode(pin, OUTPUT);
    Update();
  }

  void TurnOn() {
    _is_active = true;
    Update();
  }

  void TurnOff() {
    _is_active = false;
    Update();
  }

private:

  void Update() {
    if (!_is_active)  //lamp - switch to off
      digitalWrite(_pin, 0);
    else {
      digitalWrite(_pin, 1);
    }
  }

};
#endif
