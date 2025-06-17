

#include "HomeSpan.h" 
#include "Configuration.h"
#include "LightBulbService.h"
#include "TemperatureAndHumidityService.h"
#include "Adafruit_Sensor.h"
#include <Wire.h>

void setup() {

  Serial.begin(115200);
  Wire.setPins(SDA_PIN, SCL_PIN); // Or Wire.begin(SDA_PIN, SCL_PIN);
  Wire.begin();
  homeSpan.setLogLevel(1);
  homeSpan.enableOTA(false);
  homeSpan.enableAutoStartAP();
  homeSpan.begin(Category::Lighting,"LightBulbs");

  new SpanAccessory();  
    new LightBulbAccessoryInformation(cw1_LedPin);         // instantiate a new DEV_INFO structure that will run our custom identification routine to blink an LED on pin 13 three times
    new CCTLightBulbService(cw1_LedPin,ww1_LedPin);

  new SpanAccessory();  
    new LightBulbAccessoryInformation(cw2_LedPin);         // instantiate a new DEV_INFO structure that will run our custom identification routine to blink an LED on pin 13 three times
    new CCTLightBulbService(cw2_LedPin,ww2_LedPin);
  
    new SpanAccessory();  
      new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("Temp Sensor");
    new TemperatureSensorService();  

     new SpanAccessory();  
      new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("Temp Sensor");
    new HumiditySensorService();  

  
}

//////////////////////////////////////

void loop(){ 
  homeSpan.poll();
}

//////////////////////////////////////

// NOTE:  Once a device has been paired, it is no longer possible to trigger the Identify Characteristic from the Home App.
// Apple assumes that the identification routine is no longer needed since you can always identify the device by simply operating it.
// However, the Eve for HomeKit app DOES provide an "ID" button in the interface for each Accessory that can be used to trigger
// the identification routine for that Accessory at any time after the device has been paired.
