#ifndef POWERCYCLEDETECTOR_H
#define POWERCYCLEDETECTOR_H

#include <Arduino.h>
#include <EEPROM.h>

#if defined(ESP8266) || defined(ESP32)
  #define EEPROM_SIZE 512
  #define EEPROM_BEGIN() EEPROM.begin(EEPROM_SIZE)
  #define EEPROM_COMMIT() EEPROM.commit()
#else
  #define EEPROM_BEGIN()
  #define EEPROM_COMMIT()
#endif

class PowerCycleDetector {
public:
  PowerCycleDetector(uint16_t countAddr = 0, uint16_t timeAddr = 4)
    : _countAddr(countAddr), _timeAddr(timeAddr) {}

  void begin(uint8_t requiredCycles = 3, unsigned long timeWindowMs = 10000) {
    _requiredCycles = requiredCycles;
    _timeWindow = timeWindowMs;

    EEPROM_BEGIN();

    uint8_t cycleCount = EEPROM.read(_countAddr);
    unsigned long lastTime;
    EEPROM.get(_timeAddr, lastTime);

    unsigned long now = millis();
    bool withinWindow = (now - lastTime <= _timeWindow);

    if (withinWindow) {
      cycleCount++;
    } else {
      cycleCount = 1;
    }

    EEPROM.write(_countAddr, cycleCount);
    EEPROM.put(_timeAddr, now);
    EEPROM_COMMIT();

    if (cycleCount >= _requiredCycles) {
      _triggered = true;
      EEPROM.write(_countAddr, 0); // reset count
      EEPROM_COMMIT();
    } else {
      _triggered = false;
    }
  }

  bool wasTriggered() const {
    return _triggered;
  }

private:
  uint16_t _countAddr;
  uint16_t _timeAddr;
  uint8_t _requiredCycles = 3;
  unsigned long _timeWindow = 10000;
  bool _triggered = false;
};

#endif
