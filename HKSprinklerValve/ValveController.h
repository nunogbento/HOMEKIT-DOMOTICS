#ifndef VALVECONTROLLER_H_
#define VALVECONTROLLER_H_

#include "configuration.h"


enum ValveType {
  GENERIC_VALVE = 0,
  IRRIGATION,
  SHOWER_HEAD,
  WATER_FAUCET
};

typedef std::function<void(u_int8_t active)> Active_change_callback;
typedef std::function<void(u_int16_t remainingDuration)> Remaining_duration_change_callback;

class ValveController {
  ValveType _type;
  u_int8_t _pin;
  bool _is_active = false;
  u_int16_t _duration;
  u_int16_t _remainingDuration;
  Active_change_callback _active_change_callback;
  Remaining_duration_change_callback _remaining_duration_change_callback;
public:

  ValveController(u_int8_t pin,u_int16_t duration) {
    _pin=pin;

    pinMode(pin, OUTPUT);
    Update();
  }

  void SetDuration(u_int16_t duration) {
    _duration=duration;
  }

  void setActiveChangeCallback(Active_change_callback callback) {
      _active_change_callback = callback;
  }

  void setRemainingDurationChangeCallback(Remaining_duration_change_callback callback) {
      _remaining_duration_change_callback = callback;
  }

  void TurnOn() {
    _is_active = true;
    Update();
  }

  void TurnOff() {
    _is_active = false;
    Update();
  }
  
  
  void Loop() {
      unsigned long now = millis();
     // if (now - lastSampleTime >= measureInterval)
     // {
       
       //   if (callback)
        //    callback(currentTemperature, currentHumidity);

        
    //}
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
