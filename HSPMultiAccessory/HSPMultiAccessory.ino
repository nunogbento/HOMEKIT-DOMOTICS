

#include "HomeSpan.h" 

//////////////////////////////////////

// Below is the same DEV_LED Lightbulb Service we've used in many of the previous examples

struct DEV_LED : Service::LightBulb {

  int ledPin;
  SpanCharacteristic *power;
  
  DEV_LED(int ledPin) : Service::LightBulb(){

    power=new Characteristic::On();
    this->ledPin=ledPin;
    pinMode(ledPin,OUTPUT);    
  }

  boolean update(){            
    digitalWrite(ledPin,power->getNewVal());
    LOG0("LED %d: Power %s\n",ledPin,power->getNewVal()?"ON":"OFF");
    return(true);
  }
};

//////////////////////////////////////

// NEW: Here we derive a new class, DEV_INFO, from the Accessory Information Service

// This structure takes a single argument (ledPin), creates a name from it, and assigns
// it to the Name Characteristic.

// It also instantiates the required Identify Characteristic, and implements an update() method
// that logs a message to the Serial Monitor and blinks the associated LED three times.

// Note that in the update() method we do not bother to check which Characteristic has been updated.
// This is because the only possibility is the Identify Characteristic.

// Also, we do not need to use getNewVal() to check the value.  The Home App always sends a value of 1,
// since it is just trying to trigger the identification routine (the value itself is meaningless).

struct DEV_INFO : Service::AccessoryInformation {

  int ledPin;
  
  DEV_INFO(int ledPin) : Service::AccessoryInformation(){

    new Characteristic::Identify();
    char c[64];
    sprintf(c,"LED-%d",ledPin);
    new Characteristic::Name(c);               
    this->ledPin=ledPin;
    pinMode(ledPin,OUTPUT);    
  }

  boolean update(){
    LOG0("Running Identification for LED %d\n",ledPin);
    for(int i=0;i<3;i++){
      digitalWrite(ledPin,HIGH);
      delay(500);
      digitalWrite(ledPin,LOW);
      delay(500);
    }
    return(true);
  }
};

//////////////////////////////////////

void setup() {

  Serial.begin(115200);

  homeSpan.setLogLevel(1);
  homeSpan.begin(Category::Lighting,"HomeSpan LEDS");

// Here we replace the usual construct:

//   new SpanAccessory();
//    new Service::AccessoryInformation();  
//      new Characteristic::Identify();

// with this:

  new SpanAccessory();  
    new DEV_INFO(13);         // instantiate a new DEV_INFO structure that will run our custom identification routine to blink an LED on pin 13 three times

  new SpanAccessory();
    new DEV_INFO(16);         // Note we instantiate a new DEV_INFO structure for each Accessory in this device
    new DEV_LED(16);          // Here we instantiate the usual DEV_LED structure that controls the LED during normal operation

  new SpanAccessory();        // Here we add a second LED Accessory
    new DEV_INFO(17);               
    new DEV_LED(17);    
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
