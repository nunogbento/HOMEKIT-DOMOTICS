#ifndef PWMLED_ACCESSORY_INFO_H
#define PWMLED_ACCESSORY_INFO_H

#include "HomeSpan.h"

struct PWMLEDAccessoryInfo : Service::AccessoryInformation {

  uint8_t _pin;                    // PWM LED pin
  int _nBlinks;                    // Number of times to blink
  SpanCharacteristic *identify;    // Reference to the Identify Characteristic

  // Non-blocking blink state
  bool _blinking;                  // Currently blinking
  int _currentBlink;               // Current blink count
  bool _ledState;                  // Current LED state (on/off)
  unsigned long _lastToggleTime;   // Last time LED was toggled
  const unsigned long _blinkInterval = 500;  // 500ms on/off interval

  PWMLEDAccessoryInfo(const char *name, const char *manu, const char *sn, const char *model,
                      const char *version, uint8_t pin, int nBlinks = 3)
    : Service::AccessoryInformation() {

    new Characteristic::Name(name);
    new Characteristic::Manufacturer(manu);
    new Characteristic::SerialNumber(sn);
    new Characteristic::Model(model);
    new Characteristic::FirmwareRevision(version);
    identify = new Characteristic::Identify();

    _pin = pin;
    _nBlinks = nBlinks;
    _blinking = false;
    _currentBlink = 0;
    _ledState = false;
    _lastToggleTime = 0;
  }

  boolean update() {
    // Start the blink sequence
    _blinking = true;
    _currentBlink = 0;
    _ledState = false;
    _lastToggleTime = millis();

    // Turn on immediately for first blink (full brightness)
    ledcWrite(_pin, 1023);  // 10-bit PWM: 1023 = 100%
    _ledState = true;

    return true;
  }

  void loop() {
    if (!_blinking) return;

    unsigned long now = millis();
    if (now - _lastToggleTime >= _blinkInterval) {
      _lastToggleTime = now;

      if (_ledState) {
        // LED is ON, turn it OFF
        ledcWrite(_pin, 0);
        _ledState = false;
        _currentBlink++;

        // Check if we've completed all blinks
        if (_currentBlink >= _nBlinks) {
          _blinking = false;
        }
      } else {
        // LED is OFF, turn it ON (start next blink)
        ledcWrite(_pin, 1023);  // Full brightness
        _ledState = true;
      }
    }
  }
};

#endif
