#ifndef LEAK_SENSOR_H
#define LEAK_SENSOR_H

#include "HomeSpan.h"

struct LeakSensorAccessory : Service::LeakSensor {

    SpanCharacteristic *leakDetected;
    uint8_t _pin;
    bool _inverted;
    unsigned long _cooldownMs;      // Cooldown period in milliseconds
    unsigned long _lastTriggerTime; // Last time sensor was triggered
    bool _inCooldown;               // Currently in cooldown period

    LeakSensorAccessory(uint8_t pin, bool inverted = true, uint16_t cooldownSec = 0)
        : Service::LeakSensor() {
        // LeakDetected: 0 = Leak not detected, 1 = Leak detected
        leakDetected = new Characteristic::LeakDetected(0);
        _pin = pin;
        _inverted = inverted;
        _cooldownMs = (unsigned long)cooldownSec * 1000;
        _lastTriggerTime = 0;
        _inCooldown = false;
    }

    // Call this when pin state changes
    void setLeakDetected(bool rawPinState) {
        bool detected = _inverted ? !rawPinState : rawPinState;
        unsigned long now = millis();

        // If detection and cooldown is active, check if we're still in cooldown
        if (detected && _cooldownMs > 0) {
            if (_inCooldown) {
                // Check if cooldown has expired
                if (now - _lastTriggerTime >= _cooldownMs) {
                    _inCooldown = false;
                } else {
                    // Still in cooldown, ignore this trigger
                    return;
                }
            }
            // Start new cooldown period
            _lastTriggerTime = now;
            _inCooldown = true;
        }

        // Update HomeKit if state changed
        uint8_t newVal = detected ? 1 : 0;
        if (leakDetected->getVal() != newVal) {
            leakDetected->setVal(newVal);
        }
    }

    // Get current detection state
    bool isLeakDetected() const {
        return leakDetected->getVal() == 1;
    }

    // Get the associated pin
    uint8_t getPin() const {
        return _pin;
    }

    // Check if currently in cooldown (for debugging)
    bool isInCooldown() const {
        return _inCooldown;
    }
};

#endif // LEAK_SENSOR_H
