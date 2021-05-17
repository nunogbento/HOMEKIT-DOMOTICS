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
  COOLING = 2,
  AUTO = 3
};

enum SwingModes {
  DISABLED = 0,
  ENABLED = 1
};

typedef std::function<void(float temperature, float humidity, bool isActive, ACState currentState)> ac_callback;

class ACController {
    IRsend irsend;
    AM2320Controller sensorController;
    ac_callback callback;
    float heatingThresholdTemperature = 15;
    float coolingThresholdTemperature = 22;

    ACState currentHeaterCoolerState = OFF;
    ACState targetHeaterCoolerState = OFF;

    u_int RotationSpeed = 50;
    u_int Air_flow = 1;

    SwingModes swingMode = DISABLED;
    // 0 : low
    // 1 : mid
    // 2 : high
    // if kAc_Type = 1, 3 : change
    unsigned int ac_flow = 0;

    const uint8_t kAc_Flow_Wall[4] = {0, 2, 4, 5};

    uint32_t ac_code_to_sent;


  public:
    ACController(uint16_t irPin): irsend(irPin), sensorController() {

      sensorController.setCallback([&](float t, float h) {
        if (callback)
          callback(t, h, Active(), currentHeaterCoolerState);
      });
    }

    void begin(int sda, int scl) {
      irsend.begin();
      sensorController.begin(sda, scl);
    }

    void setCallback(ac_callback _callback) {
      callback = _callback;
    }

    float CurrentTemperature() {
      return sensorController.CurrentTemperature();
    }

    float CurrentHumidity() {
      return sensorController.CurrentHumidity();
    }

    void Loop() {
      sensorController.Loop();
    }

    void EnableSwing() {
      swingMode = ENABLED;
      if (currentHeaterCoolerState != OFF)
        Ac_Change_Air_Swing();
    }

    void DisableSwing() {
      swingMode = DISABLED;
      if (currentHeaterCoolerState != OFF)
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

      if (currentHeaterCoolerState != OFF)
        Ac_Activate();

    }

    void SetCoolingThresholdTemperature(float temperature) {
      coolingThresholdTemperature = temperature;
    }

    void SetHeatingThresholdTemperature(float temperature) {
      heatingThresholdTemperature = temperature;
    }

    ACState CurrentHeaterCoolerState() {
      return   currentHeaterCoolerState;
    }

    ACState TargettHeaterCoolerState() {
      return   targetHeaterCoolerState;
    }

    bool Active() {
      if (currentHeaterCoolerState == HEATING && CurrentTemperature() < heatingThresholdTemperature) return true;
      if (currentHeaterCoolerState == COOLING && CurrentTemperature() > coolingThresholdTemperature) return true;
      return false;
    }

    void SetTargetState(ACState newstate) {
      targetHeaterCoolerState = newstate;
      currentHeaterCoolerState = (newstate < 3) ? newstate : ((CurrentTemperature() < 15) ? HEATING : COOLING);
      switch (currentHeaterCoolerState) {
        case OFF:
          Ac_Power_Down();
          break;
        case HEATING:
          Ac_Activate();
          break;
        case COOLING:
          Ac_Activate();
          break;
        case AUTO:
          Ac_Activate();
          break;
      };
      if (callback)
        callback(CurrentTemperature(), CurrentHumidity(), Active(), currentHeaterCoolerState);
    }

  private:
    void Ac_Activate() {

      unsigned int ac_msbits1 = 8;
      unsigned int ac_msbits2 = 8;
      unsigned int ac_msbits3 = 0;
      unsigned int ac_msbits4;
      if (currentHeaterCoolerState == HEATING)
        ac_msbits4 = 4;  // heating
      else
        ac_msbits4 = 0;  // cooling
      unsigned int ac_msbits5;
      if (currentHeaterCoolerState == HEATING)
        ac_msbits5 =  (heatingThresholdTemperature < 15) ? 0 : heatingThresholdTemperature - 15;  // heating
      else
        ac_msbits5 =  (coolingThresholdTemperature < 15) ? 0 : coolingThresholdTemperature - 15;  // cooling


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

      irsend.sendLG(ac_code_to_sent, 28);
      LOG_D("AC ACTIVATE code:%d TargetTemperature:%f, TargetState:%d, Flow:%d", ac_code_to_sent, coolingThresholdTemperature, currentHeaterCoolerState, Air_flow);
    }


    void Ac_Power_Down() {
      ac_code_to_sent = 0x88C0051;
      irsend.sendLG(ac_code_to_sent, 28);
      LOG_D("sent AC off");
    }

    void Ac_Change_Air_Swing() {
      if (swingMode == ENABLED)
        ac_code_to_sent = 0x8813149;
      else
        ac_code_to_sent = 0x881315A;

      irsend.sendLG(ac_code_to_sent, 28);
      LOG_D("sent AC Swing:%d ", ac_code_to_sent);

    }


};


#endif
