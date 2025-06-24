#include "IRsend.h"
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
  bool AC_updated = false;

  LGACHeaterCoolerService(uint8_t irLedPin)
    : ac(irLedPin) {

    fanSpeed.setRange(0, 100, 25);  // Map to Auto, Low, Med, High
    sensor = new AM2320();
    ac.calibrate();
    ac.setModel(lg_ac_remote_model_t::AKB75215403);
    ac.begin();
  }

  boolean update() override {

    //Active status changed
    if (active.updated()) {
      if (active.getNewVal() == 0) {  //Off
        ac.off();
        currentState.setVal(0);
      } else {
        ac.on();
        currentState.setVal(1);
      }
      AC_updated = true;
    }


    //target status changed
    if (targetState.updated()) {
      if (targetState.getNewVal() == 0) {  //AUTO -> set idle leave to the thermostat in loop to manage
        currentState.setVal(1);
      } else if (targetState.getNewVal() == 1) {  //HEATING
        ac.setTemp((int)hTargetTemp.getVal());
        ac.setMode(kLgAcHeat);
        currentState.setVal(2);
      } else if (targetState.getNewVal() == 2) {  //COOLING
        ac.setTemp((int)cTargetTemp.getVal());
        ac.setMode(kLgAcCool);
        currentState.setVal(3);
      }
      AC_updated = true;
    }

    // Fan speed changed
    if (fanSpeed.updated()) {
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
      AC_updated = true;
    }

    //Target Temperature changed
    if (hTargetTemp.updated() && currentState.getVal() == 2) {
      ac.setTemp((int)hTargetTemp.getNewVal());
      AC_updated = true;
    }

    if (cTargetTemp.updated() && currentState.getVal() == 3) {
      ac.setTemp((int)cTargetTemp.getNewVal());
      AC_updated = true;
    }

    // Swing Changed
    if (swingMode.updated()) {
      ac.setSwingV(swingMode.getNewVal() == 1 ? true : false);
      AC_updated = true;
    }

    return true;
  }

  void loop() override {
    if (currentTemp.timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds
      if (sensor->measure()) {
        float cTemp = sensor->getTemperature();
        currentTemp.setVal(cTemp);

        // thermostat
        if (targetState.getVal() != 2 && currentState.getVal() == 1 && cTemp < hTargetTemp.getVal<float>() - T_hysteresys) {  // NEDS TO START HEATING THE PLACE
          ac.setTemp((int)hTargetTemp.getVal());
          ac.setMode(kLgAcHeat);
          currentState.setVal(2);
          AC_updated = true;
        } else if (targetState.getVal() != 1 && currentState.getVal() == 1 && cTemp > cTargetTemp.getVal<float>() + T_hysteresys) {  // NEDS TO START cooling THE PLACE
          ac.setTemp((int)cTargetTemp.getVal());
          ac.setMode(kLgAcCool);
          currentState.setVal(3);
          AC_updated = true;
        } else if ((currentState.getVal() == 2 && cTemp > hTargetTemp.getVal<float>() + T_hysteresys) || (currentState.getVal() == 3 && cTemp < cTargetTemp.getVal<float>() - T_hysteresys)) {
          currentState.setVal(1);
          AC_updated = true;
        }

      } else {  // error has occured
        int errorCode = sensor->getErrorCode();
        switch (errorCode) {
          case 1: LOG_D("ERR: Sensor is offline"); break;
          case 2: LOG_D("ERR: CRC validation failed."); break;
        };
      }
    }
    if (AC_updated) {
      Serial.println("AC Status UPDATE SENT TO IR:" + ac.toString());
      ac.send();
      AC_updated = false;
    }
  }
};