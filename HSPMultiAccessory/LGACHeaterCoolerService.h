#ifndef LGACHEATERCOOLERSERVICE_H_
#define LGACHEATERCOOLERSERVICE_H_
#include <HomeSpan.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_LG.h>

struct DEV_LG_AC_IR : Service::HeaterCooler {

  int irPin;
  IRLgAc ac;

  // Required characteristics
  Characteristic::Active active{0};                    // 0=off, 1=on
  Characteristic::CurrentHeaterCoolerState current{0}; // 0=inactive, 1=heating, 2=cooling
  Characteristic::TargetHeaterCoolerState target{1};   // 0=heat, 1=cool, 2=auto
  Characteristic::CoolingThresholdTemperature coolTemp{24};
  Characteristic::HeatingThresholdTemperature heatTemp{22};

  // Additional characteristics
  Characteristic::RotationSpeed fanSpeed{0};           // 0-100%, maps to fan level
  Characteristic::On swingMode{0};                     // 0=off, 1=on

  DEV_LG_AC_IR(int irPin) : irPin(irPin), ac(irPin) {
    // Initialize IR
    ac.begin();
    ac.setModel(LG_AC_MODEL_2);  // Some use MODEL_1

    // Set valid ranges
    coolTemp.setRange(16, 30, 1);
    heatTemp.setRange(16, 30, 1);
    fanSpeed.setRange(0, 100, 25);  // 0, 25, 50, 75, 100 (5 levels)

    Serial.printf("LG AC IR service initialized on pin %d\n", irPin);
  }

  boolean update() override {
    sendACCommand();
    return true;
  }

  void sendACCommand() {
    if (!active.getNewVal()) {
      ac.off();
      current.setVal(0);  // inactive
      Serial.println("AC OFF");
    } else {
      ac.on();

      // Set temperature + mode
      if (target.getNewVal() == 1) {
        ac.setMode(kLgAcModeCool);
        ac.setTemp(coolTemp.getNewVal());
        current.setVal(2);  // cooling
        Serial.printf("AC COOL to %.0f°C\n", coolTemp.getNewVal());
      } else if (target.getNewVal() == 0) {
        ac.setMode(kLgAcModeHeat);
        ac.setTemp(heatTemp.getNewVal());
        current.setVal(1);  // heating
        Serial.printf("AC HEAT to %.0f°C\n", heatTemp.getNewVal());
      } else {
        ac.setMode(kLgAcModeFan);
        current.setVal(3);  // fan only
        Serial.println("AC FAN ONLY");
      }

      // Set fan speed based on RotationSpeed %
      uint8_t speed = fanSpeed.getNewVal();
      if (speed == 0) ac.setFan(kLgAcFanAuto);
      else if (speed <= 25) ac.setFan(kLgAcFanLow);
      else if (speed <= 50) ac.setFan(kLgAcFanMed);
      else if (speed <= 75) ac.setFan(kLgAcFanHigh);
      else ac.setFan(kLgAcFanHigh);  // LG AC has limited steps

      Serial.printf("Fan speed set to %d%%\n", speed);

      // Set swing mode
      if (swingMode.getNewVal()) {
        ac.setSwingVertical(kLgAcSwingVAuto);
        Serial.println("Swing mode: ON");
      } else {
        ac.setSwingVertical(kLgAcSwingVOff);
        Serial.println("Swing mode: OFF");
      }

      ac.send();  // Send IR command
    }
  }

  void loop() override {
    // Could add timed updates here
  }
};



#endif
