#include <HomeSpan.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Wire.h>
#include "AM2320.h"


#define _IR_ENABLE_DEFAULT_ false
#define SEND_LG true

#define T_hysteresys 0.9

struct LGACHeaterCoolerService : Service::HeaterCooler {

  AM2320 *sensor;
  IRsend irsend;

  u_int Air_flow = 1;

  // 0 : low
  // 1 : mid
  // 2 : high
  const uint8_t kAc_Flow_Wall[4] = { 0, 2, 4, 5 };

  uint32_t ac_code_to_sent;

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
    : irsend(irLedPin) {

    fanSpeed.setRange(0, 100, 25);  // Map to Auto, Low, Med, High
    sensor = new AM2320();
    irsend.begin();
  }

  boolean update() override {

    // Fan
    int fs = fanSpeed.getNewVal();
    if (fs == 0) {
      Air_flow = 0;
    } else if (fs <= 25) {
      Air_flow = 1;
    } else if (fs <= 50) {
      Air_flow = 2;
    } else {
      Air_flow = 2;
    }

    // Swing
    if (currentState.getVal() > 1 && swingMode.getVal() != swingMode.getNewVal()) {
      Ac_Change_Air_Swing(swingMode.getNewVal() == 1 ? true : false);
    }

    return true;
  }

  void loop() override {
    if (currentTemp.timeVal() > 5000) {  // check time elapsed since last update and proceed only if greater than 5 seconds

      //Active Status
      if (targetState.getVal() == 0 && currentState.getVal() != 0) {  // shutdown now
        Ac_Power_Down();
        currentState.setVal(0);
      } else if (targetState.getVal() != 0 && currentState.getVal() == 0) {  // shutdown now        
        currentState.setVal(1);
      }

      if (sensor->measure()) {
        float cTemp = sensor->getTemperature();

        currentTemp.setVal(cTemp);
        


        
        if (currentState.getVal() != 0) {
          //Set Current Mode 
          if((targetState.getVal() == 3 && currentState.getVal() != 2 && cTemp < hTargetTemp.getVal<float>() - T_hysteresys) || (targetState.getVal() == 1 && currentState.getVal() != 2) ){
            currentState.setVal(2); //Set current Mode to Heat
          }else if((targetState.getVal() == 3 && currentState.getVal() != 3 and cTemp > cTargetTemp.getVal<float>() + T_hysteresys) || (targetState.getVal() == 2 && currentState.getVal() != 3)  ){
            currentState.setVal(3); // set Current Mode to Cool
          }else {
            currentState.setVal(1);
          }

          // Thermostat
          if (currentState.getVal() == 2 && cTemp < hTargetTemp.getVal<float>() - T_hysteresys) {  // NEDS TO START HEATING THE PLACE            
            Ac_Activate();
          } else if (currentState.getVal() == 3 && cTemp > cTargetTemp.getVal<float>() + T_hysteresys) {  // NEDS TO START cooling THE PLACE            
            Ac_Activate();
          } else if ((currentState.getVal() == 2 && cTemp > hTargetTemp.getVal<float>() + T_hysteresys) || (currentState.getVal() == 3 && cTemp < cTargetTemp.getVal<float>() - T_hysteresys)) {
            currentState.setVal(1);
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
private:

  void Ac_Activate() {

    unsigned int ac_msbits1 = 8;
    unsigned int ac_msbits2 = 8;
    unsigned int ac_msbits3 = 0;
    unsigned int ac_msbits4;
    if (currentState.getVal() == 2)
      ac_msbits4 = 4;  // heating
    else
      ac_msbits4 = 0;  // cooling


    unsigned int ac_msbits5;
    if (currentState.getVal() == 2)
      ac_msbits5 = (hTargetTemp.getVal<float>() < 15) ? 0 : hTargetTemp.getVal<float>() - 15;  // heating
    else
      ac_msbits5 = (cTargetTemp.getVal<float>() < 15) ? 0 : cTargetTemp.getVal<float>() - 15;  // cooling


    unsigned int ac_msbits6;

    if (0 <= Air_flow && Air_flow <= 2) {
      ac_msbits6 = kAc_Flow_Wall[Air_flow];
    }

    // calculating using other values
    unsigned int ac_msbits7 = (ac_msbits3 + ac_msbits4 + ac_msbits5 + ac_msbits6) & B00001111;
    ac_code_to_sent = ac_msbits1 << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits2) << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits3) << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits4) << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits5) << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits6) << 4;
    ac_code_to_sent = (ac_code_to_sent + ac_msbits7);

    irsend.sendLG(ac_code_to_sent, 28);
    LOG_D("AC ACTIVATE code:%d TargetTemperature:%f, TargetState:%d, Flow:%d", ac_code_to_sent, cTargetTemp.getVal<float>(), currentState.getVal(), Air_flow);
  }


  void Ac_Power_Down() {
    ac_code_to_sent = 0x88C0051;
    irsend.sendLG(ac_code_to_sent, 28);
    LOG_D("sent AC off");
  }

  void Ac_Change_Air_Swing(bool sMode) {
    if (sMode == true)
      ac_code_to_sent = 0x8813149;
    else
      ac_code_to_sent = 0x881315A;

    irsend.sendLG(ac_code_to_sent, 28);
    LOG_D("sent AC Swing:%d ", ac_code_to_sent);
  }
};