#ifndef VALVE_H
#define VALVE_H

#include "HomeSpan.h"
#include <functional>

typedef std::function<void(uint8_t pin, bool state)> ValveCallback;

struct ValveAccessory : Service::Valve {

    SpanCharacteristic *active;
    SpanCharacteristic *inUse;
    SpanCharacteristic *valveType;
    uint8_t _pin;
    ValveCallback _callback;

    // valveTypeValue: 0=Generic, 1=Irrigation, 2=Shower, 3=Faucet
    ValveAccessory(uint8_t pin, uint8_t valveTypeValue = 0) : Service::Valve() {
        active = new Characteristic::Active(0);
        inUse = new Characteristic::InUse(0);
        valveType = new Characteristic::ValveType(valveTypeValue);
        _pin = pin;
        _callback = nullptr;
    }

    void setCallback(ValveCallback callback) {
        _callback = callback;
    }

    // Called when HomeKit requests state change
    boolean update() override {
        bool newState = active->getNewVal();

        // Update InUse to match Active state
        inUse->setVal(newState ? 1 : 0);

        // Call callback to actually control the hardware
        if (_callback) {
            _callback(_pin, newState);
        }

        return true;
    }

    // Get current active state
    bool isActive() const {
        return active->getVal() == 1;
    }

    // Get the associated pin
    uint8_t getPin() const {
        return _pin;
    }
};

#endif // VALVE_H
