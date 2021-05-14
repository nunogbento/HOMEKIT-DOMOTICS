#ifndef ACCONTROLLER_H_
#define ACCONTROLLER_H_

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "configuration.h"
#include "AM2320Controller.h"

enum ACState {
  OFF = 0,
  HEATING = 1,
  COOLING = 2
};

enum SwingModes {
  DISABLED = 0,
  ENABLED = 1
};

class ACController {
    IRsend irsend;
    AM2320Controller sensorController;
    float CurrentTemperature = 0;
    float TargetTemperature = 0;

    ACState CurrentHeaterCoolerState = OFF;

    u_int RotationSpeed = 0;
    u_int Air_flow = 0;

    SwingModes SwingMode = DISABLED;
    // 0 : low
    // 1 : mid
    // 2 : high
    // if kAc_Type = 1, 3 : change
    unsigned int ac_flow = 0;

    const uint8_t kAc_Flow_Wall[4] = {0, 2, 4, 5};

    uint32_t ac_code_to_sent;


  public:
    ACController(uint16_t irPin): irsend(irPin), sensorController() {
      pinMode(irPin, OUTPUT);
    }
    void begin(int sda, int scl) {

      irsend.begin();
      sensorController.begin(sda, scl);
    }

    void setCallback(measurement_callback _callback) {
      sensorController.setCallback(_callback);
    }

    void Loop() {
      sensorController.Loop();
    }

    void EnableSwing() {
      SwingMode = ENABLED;
      if (CurrentHeaterCoolerState != OFF)
        Ac_Change_Air_Swing();
    }

    void DisableSwing() {
      SwingMode = DISABLED;
      if (CurrentHeaterCoolerState != OFF)
        Ac_Change_Air_Swing();
    }
    void SetRotationSpeed(u_int rotationspeed) {
      RotationSpeed = rotationspeed;
      if (RotationSpeed < 33)
        Air_flow = 0;
      else if (RotationSpeed < 66)
        Air_flow = 1;
      else
        Air_flow = 2;

      if (CurrentHeaterCoolerState != OFF)
        Ac_Activate();

    }
    void SetTargetTemperature(u_int temperature) {
      TargetTemperature = temperature;
      if (CurrentHeaterCoolerState != OFF)
        Ac_Activate();
    }

    void SetTargetState(ACState newstate) {
      CurrentHeaterCoolerState = newstate;
      switch (CurrentHeaterCoolerState) {
        case OFF:
          Ac_Power_Down();
          break;
        case HEATING:
          Ac_Activate();
          break;
        case COOLING:
          Ac_Activate();
          break;
      };
    }

  private:
    void Ac_Activate() {

      unsigned int ac_msbits1 = 8;
      unsigned int ac_msbits2 = 8;
      unsigned int ac_msbits3 = 0;
      unsigned int ac_msbits4;
      if (CurrentHeaterCoolerState == HEATING)
        ac_msbits4 = 4;  // heating
      else
        ac_msbits4 = 0;  // cooling
      unsigned int ac_msbits5 =  (TargetTemperature < 15) ? 0 : TargetTemperature - 15;
      unsigned int ac_msbits6;

      if (0 <= Air_flow && Air_flow <= 2) {
        ac_msbits6 = kAc_Flow_Wall[Air_flow];
      }

      // calculating using other values
      unsigned int ac_msbits7 = (ac_msbits3 + ac_msbits4 + ac_msbits5 +
                                 ac_msbits6) & B00001111;
      ac_code_to_sent = ac_msbits1 << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits2) << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits3) << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits4) << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits5) << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits6) << 4;
      ac_code_to_sent = (ac_code_to_sent + ac_msbits7);

      Ac_Send_Code(ac_code_to_sent);
      LOG_D("set ac_acivate:%s", ac_code_to_sent);

    }


    void Ac_Power_Down() {
      ac_code_to_sent = 0x88C0051;
      Ac_Send_Code(ac_code_to_sent);
      LOG_D("sent AC off");
    }

    void Ac_Change_Air_Swing() {
      if (SwingMode == ENABLED)
        ac_code_to_sent = 0x8813149;
      else
        ac_code_to_sent = 0x881315A;
      Ac_Send_Code(ac_code_to_sent);
      LOG_D("sent AC Swing:%s ", ac_code_to_sent);

    }

    void Ac_Send_Code(uint32_t code) {
      LOG_D("Code sent:%d",code);
      irsend.sendLG(code, 28);
    }
};


#endif
