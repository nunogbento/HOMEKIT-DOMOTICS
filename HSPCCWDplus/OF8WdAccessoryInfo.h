#ifndef OF8WD_ACCESSORY_INFO_H
#define OF8WD_ACCESSORY_INFO_H

#include "HomeSpan.h"
#include "PCCWDController.h"

struct OF8WdAccessoryInfo : Service::AccessoryInformation {

  uint8_t _address;                // OF8Wd output address
  PCCWDController* _controller;    // Reference to PCCWD controller
  int _nBlinks;                    // Number of times to blink the output
  SpanCharacteristic *identify;    // Reference to the Identify Characteristic

  // Non-blocking blink state
  bool _blinking;                  // Currently blinking
  int _currentBlink;               // Current blink count
  bool _outputState;               // Current output state (on/off)
  unsigned long _lastToggleTime;   // Last time output was toggled
  const unsigned long _blinkInterval = 500;  // 500ms on/off interval

  OF8WdAccessoryInfo(const char *name, const char *manu, const char *sn, const char *model,
                const char *version, uint8_t address, PCCWDController* controller, int nBlinks = 3)
    : Service::AccessoryInformation() {

    new Characteristic::Name(name);
    new Characteristic::Manufacturer(manu);
    new Characteristic::SerialNumber(sn);
    new Characteristic::Model(model);
    new Characteristic::FirmwareRevision(version);
    identify = new Characteristic::Identify();

    _address = address;
    _controller = controller;
    _nBlinks = nBlinks;
    _blinking = false;
    _currentBlink = 0;
    _outputState = false;
    _lastToggleTime = 0;
  }

  boolean update() {
    // Start the blink sequence
    _blinking = true;
    _currentBlink = 0;
    _outputState = false;
    _lastToggleTime = millis();

    // Turn on immediately for first blink
    _controller->setOnSate(_address, true);
    _outputState = true;

    return true;
  }

  void loop() {
    if (!_blinking) return;

    unsigned long now = millis();
    if (now - _lastToggleTime >= _blinkInterval) {
      _lastToggleTime = now;

      if (_outputState) {
        // Output is ON, turn it OFF
        _controller->setOnSate(_address, false);
        _outputState = false;
        _currentBlink++;

        // Check if we've completed all blinks
        if (_currentBlink >= _nBlinks) {
          _blinking = false;
        }
      } else {
        // Output is OFF, turn it ON (start next blink)
        _controller->setOnSate(_address, true);
        _outputState = true;
      }
    }
  }
};

#endif
