#ifndef DIMMABLE_LED_H
#define DIMMABLE_LED_H

#include "HomeSpan.h"

// Dimmable LED using PWM on a dedicated GPIO pin
// Designed for driving a MOSFET-controlled 12V LED strip
struct DimmableLED : Service::LightBulb {
    SpanCharacteristic *power;
    SpanCharacteristic *brightness;
    uint8_t _pin;
    uint8_t _channel;

    // Constructor
    // pin: GPIO pin connected to MOSFET gate
    // channel: LEDC channel (0-7 for ESP32-C6)
    DimmableLED(uint8_t pin, uint8_t channel = 0) : Service::LightBulb() {
        power = new Characteristic::On(false);
        brightness = new Characteristic::Brightness(100);
        brightness->setRange(0, 100, 1);  // 0-100% in 1% steps

        _pin = pin;
        _channel = channel;

        // Configure LEDC for PWM (ESP32 core 3.x API)
        // ledcAttach(pin, freq, resolution) returns true on success
        if (!ledcAttach(_pin, 5000, 10)) {  // 5kHz, 10-bit resolution (0-1023)
            Serial.printf("Failed to attach LEDC to GPIO%d\n", _pin);
        }
        ledcWrite(_pin, 0);  // Start with LED off

        Serial.printf("DimmableLED initialized on GPIO%d\n", _pin);
    }

    boolean update() override {
        bool isOn = power->getNewVal();
        int level = brightness->getNewVal();

        if (isOn) {
            // Map 0-100% to 0-1023 (10-bit PWM)
            uint32_t duty = (level * 1023) / 100;
            ledcWrite(_pin, duty);  // ESP32 core 3.x uses pin, not channel
            Serial.printf("LED ON: brightness=%d%% (duty=%u)\n", level, duty);
        } else {
            ledcWrite(_pin, 0);
            Serial.println("LED OFF");
        }

        return true;
    }

    // Get current state (for debugging)
    bool isOn() const {
        return power->getVal();
    }

    int getBrightness() const {
        return brightness->getVal();
    }
};

#endif // DIMMABLE_LED_H
