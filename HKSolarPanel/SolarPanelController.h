#ifndef SOLAR_PANELCONTROLLER_H_
#define SOLAR_PANELCONTROLLER_H_

#include <Arduino.h>
#include "configuration.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>


enum Current_State {
  INACTIVE = 0,
  IDLE = 1,
  HEATING = 2,
  COOLING = 3
};

const float FACTOR = 60.6F; //100A/50ma   33ohm burden  100A/1.65v

const float multiplier = 0.125;

// pooling variables
const unsigned long MeasureInterval = 2 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - MeasureInterval;  // initialize such that a reading is due the first time through loop()


typedef std::function<void(float temperature, Current_State currentState_s, Current_State currentState_e)> SolarPanel_callback;

class SolarPanelController {

    SolarPanel_callback callback;

    float targetSolarTemperature = 50;
    float targetElectricTemperature = 32;

    float currentTemperature = 32;

    Current_State currentSolarState = IDLE;
    Current_State currentElectricState = IDLE;

    // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
    OneWire oneWire;

    // Pass our oneWire reference to Dallas Temperature.
    DallasTemperature sensors;
    Adafruit_ADS1115 ads;
    WiFiClient wclient;
    PubSubClient pubSubClient;
    String TelemetryTopic;


  public:
    SolarPanelController(): oneWire(ONE_WIRE_BUS), sensors(&oneWire) , pubSubClient(wclient) {}

    void begin(int sda, int scl,String chipId) {

      TelemetryTopic =chipId+ "/from";
      ads.setGain(GAIN_ONE);        // 0.125mv

      Wire.begin( sda, scl);

      pubSubClient.setServer(MQTT_SERVER, 1883);


    }

    void setCallback(SolarPanel_callback _callback) {
      callback = _callback;
    }



    void Loop() {
      unsigned long now = millis();
      if (now - lastSampleTime >= MeasureInterval)
      {

        lastSampleTime += MeasureInterval;
        StaticJsonDocument<100> powerStatusJson;


        sensors.requestTemperatures(); // Send the command to get temperatures
        currentTemperature = sensors.getTempCByIndex(0);

        powerStatusJson["temp"] = currentTemperature;

        float currentRMS = getAmps(0);
        currentElectricState = (currentRMS > 1) ? HEATING : IDLE;
        powerStatusJson["iCh1"] = currentRMS;

        currentRMS = getAmps(1);
        currentSolarState = (currentRMS > 0.3F) ? HEATING : IDLE;
        powerStatusJson["iCh2"] = currentRMS;

        String powerStatusJsonString;
        serializeJson(powerStatusJson, powerStatusJsonString);
        Serial.println(powerStatusJsonString.c_str());
        pubSubClient.publish(TelemetryTopic.c_str(), powerStatusJsonString.c_str());
        if (callback)
          callback(currentTemperature, currentSolarState,  currentElectricState);
      }

    }




    void SetTargetElecticTemperature(float temperature) {
      targetElectricTemperature = temperature;
    }

    void SetTargetSolarTemperature(float temperature) {
      targetSolarTemperature = temperature;
    }







  private:
    float getAmps(uint8_t channel)
    {
      float voltage;
      float amps;
      float sum = 0;
      long startMillis = millis();
      int counter = 0;

      while (millis() - startMillis < 1000)
      {
        if (channel == 0)
          voltage = ads.readADC_Differential_0_1() * multiplier;
        else
          voltage = ads.readADC_Differential_2_3() * multiplier;
        amps = voltage * FACTOR;
        amps /= 1000.0;

        sum += sq(amps);
        counter++;
      }


      amps = sqrt(sum / counter);
      return amps;
    }

};


#endif
