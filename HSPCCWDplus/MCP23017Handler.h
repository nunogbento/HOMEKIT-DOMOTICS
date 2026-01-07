#ifndef MCP23017_HANDLER_H
#define MCP23017_HANDLER_H

#include <Arduino.h>
#include <Wire.h>

// MCP23017 Register addresses
#define MCP23017_IODIRA   0x00  // I/O direction register A (1=input, 0=output)
#define MCP23017_IODIRB   0x01  // I/O direction register B
#define MCP23017_IPOLA    0x02  // Input polarity register A
#define MCP23017_IPOLB    0x03  // Input polarity register B
#define MCP23017_GPINTENA 0x04  // Interrupt-on-change enable A
#define MCP23017_GPINTENB 0x05  // Interrupt-on-change enable B
#define MCP23017_DEFVALA  0x06  // Default compare value A
#define MCP23017_DEFVALB  0x07  // Default compare value B
#define MCP23017_INTCONA  0x08  // Interrupt control register A
#define MCP23017_INTCONB  0x09  // Interrupt control register B
#define MCP23017_IOCON    0x0A  // Configuration register
#define MCP23017_GPPUA    0x0C  // Pull-up resistor register A
#define MCP23017_GPPUB    0x0D  // Pull-up resistor register B
#define MCP23017_INTFA    0x0E  // Interrupt flag register A
#define MCP23017_INTFB    0x0F  // Interrupt flag register B
#define MCP23017_INTCAPA  0x10  // Interrupt captured value A
#define MCP23017_INTCAPB  0x11  // Interrupt captured value B
#define MCP23017_GPIOA    0x12  // GPIO register A
#define MCP23017_GPIOB    0x13  // GPIO register B
#define MCP23017_OLATA    0x14  // Output latch register A
#define MCP23017_OLATB    0x15  // Output latch register B

class MCP23017Handler {
public:
    MCP23017Handler(uint8_t address = 0x20, TwoWire* wire = &Wire)
        : _address(address), _wire(wire), _isConnected(false),
          _directionA(0xFF), _directionB(0xFF),  // Default all inputs
          _pullupA(0xFF), _pullupB(0xFF),        // Default all pull-ups enabled
          _outputA(0x00), _outputB(0x00),        // Default all outputs low
          _lastInputA(0x00), _lastInputB(0x00) {}

    // Initialize the MCP23017
    bool begin(int sdaPin = -1, int sclPin = -1) {
        if (sdaPin >= 0 && sclPin >= 0) {
            _wire->begin(sdaPin, sclPin);
        } else {
            _wire->begin();
        }

        // Check if device is present
        _wire->beginTransmission(_address);
        if (_wire->endTransmission() != 0) {
            _isConnected = false;
            return false;
        }

        _isConnected = true;

        // Configure IOCON register
        // BANK=0, MIRROR=0, SEQOP=0, DISSLW=0, HAEN=0, ODR=0, INTPOL=0
        writeRegister(MCP23017_IOCON, 0x00);

        // Set all pins as inputs with pull-ups (default safe state)
        writeRegister(MCP23017_IODIRA, 0xFF);
        writeRegister(MCP23017_IODIRB, 0xFF);
        writeRegister(MCP23017_GPPUA, 0xFF);
        writeRegister(MCP23017_GPPUB, 0xFF);

        // Read initial state
        _lastInputA = readRegister(MCP23017_GPIOA);
        _lastInputB = readRegister(MCP23017_GPIOB);

        return true;
    }

    // Check if MCP23017 is connected
    bool isConnected() const {
        return _isConnected;
    }

    // Configure a single pin
    // pin: 0-15 (0-7 = GPA0-7, 8-15 = GPB0-7)
    // isOutput: true for output, false for input
    // pullup: enable internal pull-up (only for inputs)
    void pinMode(uint8_t pin, bool isOutput, bool pullup = true) {
        if (pin > 15) return;

        if (pin < 8) {
            // Port A (pins 0-7)
            if (isOutput) {
                _directionA &= ~(1 << pin);  // Clear bit = output
                _pullupA &= ~(1 << pin);     // No pull-up for outputs
            } else {
                _directionA |= (1 << pin);   // Set bit = input
                if (pullup) {
                    _pullupA |= (1 << pin);
                } else {
                    _pullupA &= ~(1 << pin);
                }
            }
            writeRegister(MCP23017_IODIRA, _directionA);
            writeRegister(MCP23017_GPPUA, _pullupA);
        } else {
            // Port B (pins 8-15)
            uint8_t bit = pin - 8;
            if (isOutput) {
                _directionB &= ~(1 << bit);
                _pullupB &= ~(1 << bit);
            } else {
                _directionB |= (1 << bit);
                if (pullup) {
                    _pullupB |= (1 << bit);
                } else {
                    _pullupB &= ~(1 << bit);
                }
            }
            writeRegister(MCP23017_IODIRB, _directionB);
            writeRegister(MCP23017_GPPUB, _pullupB);
        }
    }

    // Read a single pin (returns true if HIGH)
    bool digitalRead(uint8_t pin) {
        if (pin > 15) return false;

        if (pin < 8) {
            uint8_t val = readRegister(MCP23017_GPIOA);
            return (val & (1 << pin)) != 0;
        } else {
            uint8_t val = readRegister(MCP23017_GPIOB);
            return (val & (1 << (pin - 8))) != 0;
        }
    }

    // Write a single pin
    void digitalWrite(uint8_t pin, bool value) {
        if (pin > 15) return;

        if (pin < 8) {
            if (value) {
                _outputA |= (1 << pin);
            } else {
                _outputA &= ~(1 << pin);
            }
            writeRegister(MCP23017_GPIOA, _outputA);
        } else {
            uint8_t bit = pin - 8;
            if (value) {
                _outputB |= (1 << bit);
            } else {
                _outputB &= ~(1 << bit);
            }
            writeRegister(MCP23017_GPIOB, _outputB);
        }
    }

    // Read all 16 pins at once (returns 16-bit value)
    uint16_t readAll() {
        uint8_t a = readRegister(MCP23017_GPIOA);
        uint8_t b = readRegister(MCP23017_GPIOB);
        return (uint16_t)a | ((uint16_t)b << 8);
    }

    // Write all 16 pins at once
    void writeAll(uint16_t value) {
        _outputA = value & 0xFF;
        _outputB = (value >> 8) & 0xFF;
        writeRegister(MCP23017_GPIOA, _outputA);
        writeRegister(MCP23017_GPIOB, _outputB);
    }

    // Check for input changes (call in loop)
    // Returns bitmask of changed pins (1 = changed)
    uint16_t checkChanges() {
        uint8_t newA = readRegister(MCP23017_GPIOA);
        uint8_t newB = readRegister(MCP23017_GPIOB);

        uint8_t changedA = newA ^ _lastInputA;
        uint8_t changedB = newB ^ _lastInputB;

        _lastInputA = newA;
        _lastInputB = newB;

        return (uint16_t)changedA | ((uint16_t)changedB << 8);
    }

    // Get last read input values
    uint16_t getLastInputs() const {
        return (uint16_t)_lastInputA | ((uint16_t)_lastInputB << 8);
    }

    // Get pin state from last read (avoids I2C transaction)
    bool getPinState(uint8_t pin) const {
        if (pin > 15) return false;
        if (pin < 8) {
            return (_lastInputA & (1 << pin)) != 0;
        } else {
            return (_lastInputB & (1 << (pin - 8))) != 0;
        }
    }

    // Get I2C address
    uint8_t getAddress() const {
        return _address;
    }

private:
    uint8_t _address;
    TwoWire* _wire;
    bool _isConnected;

    uint8_t _directionA, _directionB;
    uint8_t _pullupA, _pullupB;
    uint8_t _outputA, _outputB;
    uint8_t _lastInputA, _lastInputB;

    void writeRegister(uint8_t reg, uint8_t value) {
        _wire->beginTransmission(_address);
        _wire->write(reg);
        _wire->write(value);
        _wire->endTransmission();
    }

    uint8_t readRegister(uint8_t reg) {
        _wire->beginTransmission(_address);
        _wire->write(reg);
        _wire->endTransmission();
        _wire->requestFrom(_address, (uint8_t)1);
        return _wire->read();
    }
};

#endif // MCP23017_HANDLER_H
