#include <HomeSpan.h>
#include <ir_LG.h>
#include <Wire.h>
#include "AM2320.h"


#define _IR_ENABLE_DEFAULT_ false
#define SEND_LG true

#define T_hysteresys 0.9

struct LGACHeaterCoolerService : Service::HeaterCooler {

  AM2320 *sensor;
  IRLgAc ac;

  Characteristic::Active active{ 0 };
  Characteristic::CurrentHeaterCoolerState currentState{ 0 };  // IDLE
  Characteristic::TargetHeaterCoolerState targetState{ 0 };    // COOL
  Characteristic::CurrentTemperature currentTemp{ 25 };
  Characteristic::CoolingThresholdTemperature cTargetTemp{ 24 };
  Characteristic::HeatingThresholdTemperature hTargetTemp{ 15 };

  Characteristic::RotationSpeed fanSpeed{ 0 };  // 0–100
  Characteristic::SwingMode swingMode{ 0 };     // 0=Off, 1=On

  unsigned long lastRead = 0;

  LGACHeaterCoolerService(uint8_t irLedPin)
    : ac(irLedPin) {
    fanSpeed.setRange(0, 100, 25);  // Map to Auto, Low, Med, High
    sensor = new AM2320();
  }

  boolean update() override {


    if ((int)targetState.getNewVal() == 0) {
      Serial.println("Turning OFF AC");
      ac.off();
      ac.send();
      currentState.setVal(0);
    }else{
      currentState.setVal(1);
    }
    
   
    // Fan
    int fs = fanSpeed.getNewVal();
    if (fs == 0) {
      ac.setFan(kLgAcFanAuto);
    } else if (fs <= 25) {
      ac.setFan(kLgAcFanLow);
    } else if (fs <= 50) {
      ac.setFan(kLgAcFanMedium);
    } else {
      ac.setFan(kLgAcFanHigh);
    }

    // Swing
    ac.setSwingV(swingMode.getNewVal() == 1 ? true : false);

    return true;
  }

  void loop() override {
    if (currentTemp.timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds

      if (sensor->measure()) {
        float cTemp = sensor->getTemperature();

        currentTemp.setVal(cTemp);

        // Thermostat
        if (currentState.getVal() != 0) {  //AC is not off

          if (targetState.getVal() != 2 && currentState.getVal() !=2 and cTemp < hTargetTemp.getVal<float>() - T_hysteresys) {  // NEDS TO START HEATING THE PLACE
            ac.on();
            ac.setTemp((int)hTargetTemp.getVal());
            ac.setMode(kLgAcHeat);
            currentState.setVal(2);
            //active.setVal(1);
            ac.send();
          } else if (targetState.getVal() != 1 && currentState.getVal() !=3 and cTemp > cTargetTemp.getVal<float>() + T_hysteresys) {  // NEDS TO START cooling THE PLACE
            ac.on();
            ac.setTemp((int)cTargetTemp.getVal());
            ac.setMode(kLgAcCool);
            //active.setVal(1);
            currentState.setVal(3);
            ac.send();
          } else if ((currentState.getVal() == 2 && cTemp > hTargetTemp.getVal<float>() + T_hysteresys) || (currentState.getVal() == 3 && cTemp < cTargetTemp.getVal<float>() - T_hysteresys)) {
            ac.off();
            //active.setVal(0);
            currentState.setVal(1);
            ac.send();
          }
        }

      } else {  // error has occured
        int errorCode = sensor->getErrorCode();
        switch (errorCode) {
          case 1: LOG_D("ERR: Sensor is offline"); break;
          case 2: LOG_D("ERR: CRC validation failed."); break;
        }
      }
      
    }
  }
};