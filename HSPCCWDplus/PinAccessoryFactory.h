#ifndef PIN_ACCESSORY_FACTORY_H
#define PIN_ACCESSORY_FACTORY_H

#include <Arduino.h>
#include "HomeSpan.h"
#include "ConfigManager.h"
#include "MCP23017Handler.h"
#include "DEV_Identify.h"
#include "MotionSensor.h"
#include "LeakSensor.h"
#include "SmokeSensor.h"
#include "CarbonDioxide.h"
#include "Valve.h"

class PinAccessoryFactory {
public:
    PinAccessoryFactory() : _mcp(nullptr) {
        for (int i = 0; i < NUM_PINS; i++) {
            _motionSensors[i] = nullptr;
            _leakSensors[i] = nullptr;
            _smokeSensors[i] = nullptr;
            _co2Sensors[i] = nullptr;
            _valves[i] = nullptr;
        }
    }

    // Create all accessories based on configuration
    void createAccessories(ConfigManager& config, MCP23017Handler& mcp) {
        _mcp = &mcp;

        const PinConfig* pins = config.getAllPins();

        for (int i = 0; i < NUM_PINS; i++) {
            const PinConfig& pinConfig = pins[i];

            if (pinConfig.type == PinType::UNUSED) {
                continue;
            }

            // Configure MCP23017 pin direction
            bool isOutput = pinConfig.isOutput();
            mcp.pinMode(i, isOutput, !isOutput);  // pullup only for inputs

            // Create HomeSpan accessory
            new SpanAccessory();
            new DEV_Identify(
                pinConfig.name.c_str(),
                "MORDOMUS",
                "000000",
                getAccessoryModel(pinConfig.type),
                "0.1",
                0
            );

            switch (pinConfig.type) {
                case PinType::MOTION:
                    _motionSensors[i] = new MotionSensorAccessory(i, pinConfig.inverted, pinConfig.cooldown);
                    break;

                case PinType::LEAK:
                    _leakSensors[i] = new LeakSensorAccessory(i, pinConfig.inverted, pinConfig.cooldown);
                    break;

                case PinType::SMOKE:
                    _smokeSensors[i] = new SmokeSensorAccessory(i, pinConfig.inverted, pinConfig.cooldown);
                    break;

                case PinType::CO2:
                    _co2Sensors[i] = new CarbonDioxideSensorAccessory(i, pinConfig.inverted, pinConfig.cooldown);
                    break;

                case PinType::VALVE:
                    _valves[i] = new ValveAccessory(i, pinConfig.valveType);
                    _valves[i]->setCallback([this](uint8_t pin, bool state) {
                        if (_mcp) {
                            _mcp->digitalWrite(pin, state);
                        }
                    });
                    break;

                default:
                    break;
            }
        }
    }

    // Update sensor states from MCP23017 inputs
    // Call this in loop() to poll for changes
    void updateSensorStates() {
        if (!_mcp) return;

        uint16_t changes = _mcp->checkChanges();
        if (changes == 0) return;

        uint16_t currentState = _mcp->getLastInputs();

        for (int i = 0; i < NUM_PINS; i++) {
            if (!(changes & (1 << i))) continue;

            bool pinState = (currentState & (1 << i)) != 0;

            if (_motionSensors[i]) {
                _motionSensors[i]->setDetected(pinState);
            }
            if (_leakSensors[i]) {
                _leakSensors[i]->setLeakDetected(pinState);
            }
            if (_smokeSensors[i]) {
                _smokeSensors[i]->setSmokeDetected(pinState);
            }
            if (_co2Sensors[i]) {
                _co2Sensors[i]->setCO2Detected(pinState);
            }
        }
    }

    // Get accessory by pin number (for debugging)
    MotionSensorAccessory* getMotionSensor(uint8_t pin) {
        return (pin < NUM_PINS) ? _motionSensors[pin] : nullptr;
    }

    LeakSensorAccessory* getLeakSensor(uint8_t pin) {
        return (pin < NUM_PINS) ? _leakSensors[pin] : nullptr;
    }

    SmokeSensorAccessory* getSmokeSensor(uint8_t pin) {
        return (pin < NUM_PINS) ? _smokeSensors[pin] : nullptr;
    }

    CarbonDioxideSensorAccessory* getCO2Sensor(uint8_t pin) {
        return (pin < NUM_PINS) ? _co2Sensors[pin] : nullptr;
    }

    ValveAccessory* getValve(uint8_t pin) {
        return (pin < NUM_PINS) ? _valves[pin] : nullptr;
    }

private:
    MCP23017Handler* _mcp;

    // Arrays to store accessory pointers by pin number
    MotionSensorAccessory* _motionSensors[NUM_PINS];
    LeakSensorAccessory* _leakSensors[NUM_PINS];
    SmokeSensorAccessory* _smokeSensors[NUM_PINS];
    CarbonDioxideSensorAccessory* _co2Sensors[NUM_PINS];
    ValveAccessory* _valves[NUM_PINS];

    const char* getAccessoryModel(PinType type) {
        switch (type) {
            case PinType::MOTION: return "Motion Sensor";
            case PinType::LEAK: return "Leak Sensor";
            case PinType::SMOKE: return "Smoke Sensor";
            case PinType::CO2: return "CO2 Sensor";
            case PinType::VALVE: return "Valve";
            default: return "Unknown";
        }
    }
};

#endif // PIN_ACCESSORY_FACTORY_H
