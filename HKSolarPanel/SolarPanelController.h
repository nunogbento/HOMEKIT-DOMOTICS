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



enum TARGET_C_H_State {
  AUTO = 0,
  HEAT = 1,
  COOL = 2,
};

enum CURRENT_C_H_State {
  INACTIVE = 0,
  IDLE = 1,
  HEATING = 2,
  COOLING = 3
};

const float FACTOR = 60.6F; //100A/50ma   33ohm burden  100A/1.65v

const float multiplier = 0.125;



typedef std::function<void(float temperature, CURRENT_C_H_State currentState_s, CURRENT_C_H_State currentState_e,float iE,float iS)> SolarPanel_callback;

class SolarPanelController {

    SolarPanel_callback callback;
    String _chipId;
    float adcSamples_s[SAMPLE_BUFFER_SIZE];
    float adcSamples_e[SAMPLE_BUFFER_SIZE];
    int sampleindex = 0;
    float currentTemperature = 0;

    long lastMeasureTime = 0;
    long lastSampleTime = 0;

    CURRENT_C_H_State currentSolarState = IDLE;
    CURRENT_C_H_State currentElectricState = IDLE;

    // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
    OneWire oneWire;

    // Pass our oneWire reference to Dallas Temperature.
    DallasTemperature sensors;
    Adafruit_ADS1115 ads;
   
    String TelemetryTopic;


  public:
    SolarPanelController(): oneWire(ONE_WIRE_BUS), sensors(&oneWire)  {}

    void begin(String chipId) {

      _chipId = chipId;


      sensors.begin();

      if (ads.begin()) {
        ads.setGain(GAIN_ONE);// 0.125mv
        LOG_D("adc Intialized.");
      }

     


    }

    void setCallback(SolarPanel_callback _callback) {
      callback = _callback;
    }



    void Loop() {

      unsigned long now = millis();


      if (now - lastSampleTime >= SAMPLE_INTERVAL)
      {
        lastSampleTime=now;
        collectSamples();
      }

      if (now - lastMeasureTime >= MEASUREMENT_INTERVAL)
      {

        lastMeasureTime = now;
        

        sensors.requestTemperatures(); // Send the command to get temperatures
        currentTemperature = sensors.getTempCByIndex(0);

       

        LOG_D("Samples index: %i", sampleindex);
        float currentRMS_e = getAmps(0);

        LOG_D("Amps calculated Electric: %f", currentRMS_e);
        currentElectricState = (currentRMS_e > 1.0F) ? HEATING : IDLE;
       

        float currentRMS_s = getAmps(1);
        LOG_D("Amps calculated Solar: %f", currentRMS_s);
        currentSolarState = (currentRMS_s > 0.1F) ? HEATING : IDLE;
      
       
        if (callback)
          callback(currentTemperature, currentSolarState,  currentElectricState,currentRMS_e,currentRMS_s);

        
      }

    }



  private:


    float getAmps(uint8_t channel)
    {
      float sum = 0;

      for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sum += sq((channel == 0) ? adcSamples_e[i] : adcSamples_s[i]);
      }
      return sqrt(sum / SAMPLE_BUFFER_SIZE);
    }


    void collectSamples() {
      adcSamples_e[sampleindex] = ads.readADC_Differential_0_1() * multiplier * FACTOR / 1000.0;
      adcSamples_s[sampleindex] = ads.readADC_Differential_2_3() * multiplier * FACTOR / 1000.0;
      sampleindex = (sampleindex + 1) % SAMPLE_BUFFER_SIZE;
    }

};


#endif
