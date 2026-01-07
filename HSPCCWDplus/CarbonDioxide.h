#ifndef CARBON_DIOXIDE_H
#define CARBON_DIOXIDE_H

#include "HomeSpan.h"

struct CarbonDioxideSensorAccessory : Service::CarbonDioxideSensor {

    SpanCharacteristic *co2Detected;
    uint8_t _pin;
    bool _inverted;
    unsigned long _cooldownMs;      // Cooldown period in milliseconds
    unsigned long _lastTriggerTime; // Last time sensor was triggered
    bool _inCooldown;               // Currently in cooldown period

    CarbonDioxideSensorAccessory(uint8_t pin, bool inverted = true, uint16_t cooldownSec = 0)
        : Service::CarbonDioxideSensor() {
        // CarbonDioxideDetected: 0 = CO2 levels normal, 1 = CO2 levels abnormal
        co2Detected = new Characteristic::CarbonDioxideDetected(0);
        _pin = pin;
        _inverted = inverted;
        _cooldownMs = (unsigned long)cooldownSec * 1000;
        _lastTriggerTime = 0;
        _inCooldown = false;
    }

    // Call this when pin state changes
    void setCO2Detected(bool rawPinState) {
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
        if (co2Detected->getVal() != newVal) {
            co2Detected->setVal(newVal);
        }
    }

    // Get current detection state
    bool isCO2Detected() const {
        return co2Detected->getVal() == 1;
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

#endif // CARBON_DIOXIDE_H
