#ifndef CONTACT_SENSOR_H
#define CONTACT_SENSOR_H

#include "HomeSpan.h"

struct ContactSensorAccessory : Service::ContactSensor {

    SpanCharacteristic *contactState;
    uint8_t _pin;
    bool _inverted;
    unsigned long _cooldownMs;      // Cooldown period in milliseconds
    unsigned long _lastTriggerTime; // Last time sensor was triggered
    bool _inCooldown;               // Currently in cooldown period

    ContactSensorAccessory(uint8_t pin, bool inverted = true, uint16_t cooldownSec = 0)
        : Service::ContactSensor() {
        // ContactSensorState: 0 = Contact detected (closed), 1 = Contact not detected (open)
        contactState = new Characteristic::ContactSensorState(0);
        _pin = pin;
        _inverted = inverted;
        _cooldownMs = (unsigned long)cooldownSec * 1000;
        _lastTriggerTime = 0;
        _inCooldown = false;
    }

    // Call this when pin state changes
    void setContactState(bool rawPinState) {
        bool open = _inverted ? !rawPinState : rawPinState;
        unsigned long now = millis();

        // If cooldown is enabled
        if (_cooldownMs > 0) {
            if (_inCooldown) {
                // Check if cooldown has expired
                if (now - _lastTriggerTime >= _cooldownMs) {
                    _inCooldown = false;
                    // Cooldown expired, now follow actual sensor state
                } else {
                    // Still in cooldown - keep sensor active, ignore state changes
                    return;
                }
            }

            // New detection starts cooldown
            if (open && !_inCooldown) {
                _lastTriggerTime = now;
                _inCooldown = true;
            }
        }

        // Update HomeKit if state changed
        uint8_t newVal = open ? 1 : 0;
        if (contactState->getVal() != newVal) {
            contactState->setVal(newVal);
        }
    }

    // Get current contact state
    bool isOpen() const {
        return contactState->getVal() == 1;
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

#endif // CONTACT_SENSOR_H
