#ifndef VALVECONTROLLER_H_
#define VALVECONTROLLER_H_

#include "configuration.h"

#define SecondsInMillis 1000

typedef std::function<void(u_int8_t in_use)> in_use_change_callback;
typedef std::function<void(u_int8_t active)> Active_change_callback;
typedef std::function<void(u_int32_t remainingDuration)> Remaining_duration_change_callback;

class ValveController {
  unsigned long startMillis = 0;
  u_int8_t _pin;
  u_int8_t _is_active = 0;
  u_int8_t _in_use = 0;
  u_int32_t _duration;
  u_int32_t _remainingDuration=0;
  bool _noTimer = false;
  in_use_change_callback _in_use_change_callback;
  Active_change_callback _active_change_callback;
  Remaining_duration_change_callback _remaining_duration_change_callback;
public:

  ValveController(u_int8_t pin, u_int32_t duration) {
    _pin = pin;
    pinMode(pin, OUTPUT);
    Update();
    _duration = duration;
    startMillis = millis();
  }

  void SetDuration(u_int32_t duration) {
    _duration = duration;
  }

  void setActiveChangeCallback(Active_change_callback callback) {
    _active_change_callback = callback;
  }

  void setInUseChangeCallback(in_use_change_callback callback) {
    _in_use_change_callback = callback;
  }

  void setRemainingDurationChangeCallback(Remaining_duration_change_callback callback) {
    _remaining_duration_change_callback = callback;
  }

  void TurnOn(bool noTimer = false) {
    _noTimer = noTimer;
    _is_active = 1;
  }

  void TurnOff() {
    _is_active = 0;
  }

  void Loop() {
    unsigned long nowMillis = millis();

    if (nowMillis - startMillis >= SecondsInMillis)  // every second
    {
      if (_is_active != _in_use) {
        Update();
        if (_in_use_change_callback)
          _in_use_change_callback(_in_use);
      }

      if (_is_active && _remainingDuration > 0) {
        _remainingDuration--;
        if (_remaining_duration_change_callback && _remainingDuration % 60 == 0)  // notify hk every minute
          _remaining_duration_change_callback(_remainingDuration);
      } else if (!_noTimer && _is_active) {
        _is_active = 0;
        if (_active_change_callback)
          _active_change_callback(_is_active);
      }
      startMillis = nowMillis;
    }
  }

private:

  void Update() {
    if (_is_active == 0) {  //lamp - switch to off
      digitalWrite(_pin, 0);
      _remainingDuration = 0;
    } else {
      digitalWrite(_pin, 1);
      _remainingDuration = _duration;
    }
    if (_remaining_duration_change_callback)  // notify hk every minute
      _remaining_duration_change_callback(_remainingDuration);
    _in_use = _is_active;
  }
};
#endif
