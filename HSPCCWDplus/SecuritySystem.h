#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include "HomeSpan.h"
#include "ConfigManager.h"
#include "MCP23017Handler.h"

// HomeKit SecuritySystem states
#define SECURITY_STATE_STAY_ARM     0
#define SECURITY_STATE_AWAY_ARM     1
#define SECURITY_STATE_NIGHT_ARM    2
#define SECURITY_STATE_DISARMED     3
#define SECURITY_STATE_TRIGGERED    4

class SecuritySystemService : public Service::SecuritySystem {
private:
    SpanCharacteristic* _currentState;
    SpanCharacteristic* _targetState;

    MCP23017Handler* _mcp;
    const SecuritySystemConfig* _config;

    // State machine variables
    uint8_t _pendingTarget;        // Target state during exit delay
    unsigned long _armingStartTime;    // When arming began (for exit delay)
    unsigned long _triggerStartTime;   // When sensor triggered (for entry delay)
    bool _isArming;                    // In exit delay phase
    bool _entryDelayActive;            // In entry delay phase
    uint8_t _triggeringPin;            // Pin that triggered entry delay

public:
    SecuritySystemService(MCP23017Handler* mcp, const SecuritySystemConfig* config)
        : Service::SecuritySystem(),
          _mcp(mcp),
          _config(config),
          _pendingTarget(SECURITY_STATE_DISARMED),
          _armingStartTime(0),
          _triggerStartTime(0),
          _isArming(false),
          _entryDelayActive(false),
          _triggeringPin(0xFF)
    {
        _currentState = new Characteristic::SecuritySystemCurrentState(SECURITY_STATE_DISARMED);
        _targetState = new Characteristic::SecuritySystemTargetState(SECURITY_STATE_DISARMED);

        LOG_D("SecuritySystem service created");
    }

    // Called when HomeKit sets target state
    boolean update() override {
        uint8_t newTarget = _targetState->getNewVal();
        uint8_t currentState = _currentState->getVal();

        LOG_D("SecuritySystem: target changed to %d (current: %d)", newTarget, currentState);

        // If disarming
        if (newTarget == SECURITY_STATE_DISARMED) {
            // Cancel any pending arm or entry delay
            _isArming = false;
            _entryDelayActive = false;
            _triggeringPin = 0xFF;

            // Turn off siren
            activateSiren(false);

            // Set current state immediately
            _currentState->setVal(SECURITY_STATE_DISARMED);
            LOG_D("SecuritySystem: DISARMED");
            return true;
        }

        // If already triggered, must disarm first
        if (currentState == SECURITY_STATE_TRIGGERED) {
            LOG_D("SecuritySystem: Cannot arm while triggered");
            _targetState->setVal(SECURITY_STATE_DISARMED);
            return true;
        }

        // Start arming with exit delay
        if (_config->exitDelaySeconds > 0) {
            _pendingTarget = newTarget;
            _isArming = true;
            _armingStartTime = millis();
            LOG_D("SecuritySystem: Arming with %d second exit delay", _config->exitDelaySeconds);
        } else {
            // No exit delay, arm immediately
            _currentState->setVal(newTarget);
            LOG_D("SecuritySystem: Armed immediately to state %d", newTarget);
        }

        return true;
    }

    // Called from main loop to handle timing
    void loop() override {
        // Handle exit delay (arming countdown)
        if (_isArming) {
            unsigned long elapsed = millis() - _armingStartTime;
            if (elapsed >= _config->exitDelaySeconds * 1000UL) {
                _isArming = false;
                _currentState->setVal(_pendingTarget);
                LOG_D("SecuritySystem: Exit delay complete, now armed (state %d)", _pendingTarget);
            }
            return;  // Don't check sensors during exit delay
        }

        // Handle entry delay (alarm countdown)
        if (_entryDelayActive) {
            unsigned long elapsed = millis() - _triggerStartTime;
            if (elapsed >= _config->entryDelaySeconds * 1000UL) {
                _entryDelayActive = false;
                triggerAlarm();
                LOG_D("SecuritySystem: Entry delay expired, ALARM TRIGGERED");
            }
        }
    }

    // Called when a sensor state changes
    void sensorTriggered(uint8_t pin, bool active) {
        // Ignore if disarmed
        uint8_t currentState = _currentState->getVal();
        if (currentState == SECURITY_STATE_DISARMED ||
            currentState == SECURITY_STATE_TRIGGERED) {
            return;
        }

        // Ignore if in exit delay (arming)
        if (_isArming) {
            return;
        }

        // Ignore if sensor is not active (deactivation doesn't trigger alarm)
        if (!active) {
            return;
        }

        // Find trigger config for this pin
        const SecuritySensorTrigger* trigger = nullptr;
        for (int i = 0; i < _config->triggerCount; i++) {
            if (_config->triggers[i].pin == pin) {
                trigger = &_config->triggers[i];
                break;
            }
        }

        if (!trigger) {
            return;  // Pin not configured as trigger
        }

        // Check if this sensor triggers in current mode
        bool shouldTrigger = false;
        switch (currentState) {
            case SECURITY_STATE_STAY_ARM:
                shouldTrigger = trigger->triggerHome;
                break;
            case SECURITY_STATE_AWAY_ARM:
                shouldTrigger = trigger->triggerAway;
                break;
            case SECURITY_STATE_NIGHT_ARM:
                shouldTrigger = trigger->triggerNight;
                break;
        }

        if (!shouldTrigger) {
            LOG_D("SecuritySystem: Sensor pin %d not active for current mode %d", pin, currentState);
            return;
        }

        LOG_D("SecuritySystem: Sensor pin %d triggered in mode %d", pin, currentState);

        // If already in entry delay, don't restart timer
        if (_entryDelayActive) {
            return;
        }

        // Check if this is an entry point (has delay) or immediate
        if (trigger->isEntryPoint && _config->entryDelaySeconds > 0) {
            // Start entry delay
            _entryDelayActive = true;
            _triggerStartTime = millis();
            _triggeringPin = pin;
            LOG_D("SecuritySystem: Entry delay started (%d seconds)", _config->entryDelaySeconds);
        } else {
            // Immediate trigger
            triggerAlarm();
        }
    }

    // Get current state for external queries
    uint8_t getCurrentState() const {
        return _currentState->getVal();
    }

    // Check if in entry delay
    bool isInEntryDelay() const {
        return _entryDelayActive;
    }

    // Check if arming (exit delay)
    bool isArming() const {
        return _isArming;
    }

private:
    void triggerAlarm() {
        _currentState->setVal(SECURITY_STATE_TRIGGERED);
        _targetState->setVal(SECURITY_STATE_DISARMED);  // HomeKit expects target to be disarm when triggered
        activateSiren(true);
        LOG_D("SecuritySystem: ALARM TRIGGERED!");
    }

    void activateSiren(bool on) {
        if (!_mcp || _config->sirenPin == 0xFF) {
            return;  // No siren configured
        }

        // Configure pin as output if needed and set state
        _mcp->pinMode(_config->sirenPin, true, false);  // isOutput=true, pullup=false
        _mcp->digitalWrite(_config->sirenPin, on);
        LOG_D("SecuritySystem: Siren %s (pin %d)", on ? "ON" : "OFF", _config->sirenPin);
    }
};

#endif // SECURITY_SYSTEM_H
