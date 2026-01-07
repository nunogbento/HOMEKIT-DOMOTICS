#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include "HomeSpan.h"

struct MotionSensorAccessory : Service::MotionSensor {

    SpanCharacteristic *motionDetected;
    uint8_t _pin;
    bool _inverted;
    unsigned long _cooldownMs;      // Cooldown period in milliseconds
    unsigned long _lastTriggerTime; // Last time sensor was triggered
    bool _inCooldown;               // Currently in cooldown period

    MotionSensorAccessory(uint8_t pin, bool inverted = true, uint16_t cooldownSec = 0)
        : Service::MotionSensor() {
        motionDetected = new Characteristic::MotionDetected(false);
        _pin = pin;
        _inverted = inverted;
        _cooldownMs = (unsigned long)cooldownSec * 1000;
        _lastTriggerTime = 0;
        _inCooldown = false;
    }

    // Call this when pin state changes
    void setDetected(bool rawPinState) {
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
        if (motionDetected->getVal() != detected) {
            motionDetected->setVal(detected);
        }
    }

    // Get current detection state
    bool isDetected() const {
        return motionDetected->getVal();
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

#endif // MOTION_SENSOR_H
