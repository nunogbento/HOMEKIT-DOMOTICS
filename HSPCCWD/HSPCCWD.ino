
#include "HomeSpan.h"
#include "PCCWDController.h"
#include "OF8WdOutput.h"
#include "IN10wDStatelessProgrammableSwitch.h"
#include "DEV_Identify.h"



IN10wDStatelessProgrammableSwitch* switches[10];

PCCWDController pccwdController(Serial1);

void setup() {

  Serial.begin(115200);
  Serial1.begin(14400);


  homeSpan.setStatusPin(8);
  homeSpan.setControlPin(0);
  homeSpan.setApSSID("PCCWDSETUP");
  homeSpan.setApPassword("");
  
  homeSpan.begin(Category::Bridges, "PCCWD","PCCWD","PCCWD Bridge");      

  // We begin by creating a Bridge Accessory, which look just like any other Accessory,
  // except that is only contains DEV_Identify (which is derived from AccessoryInformation)
  // and HAPProtcolInformation (required).  Note that HomeKit will still call the identify
  // update() routine upon pairing, so we specify the number of blinks to be 3.

  new SpanAccessory();
  new DEV_Identify("PCCWD", "MORDOMUS", "000000", "PCCWD", "0.1", 3);
  new Service::HAPProtocolInformation();
  new Characteristic::Version("1.1.0");

  // Now we simply repeat the definitions of the previous LED Accessories, as per Example 7, with two exceptions:
  // 1) We no longer need to include the HAPProtocolInformation Service.
  // 2) We will set the number of blinks to zero, so that only the bridge accessory will cause the Built-In
  //    LED to blink. This becomes especially important if you had 20 Accessories defined and needed to wait a
  //    minute or more for all the blinking to finish while pairing.
  for (int i = 0; i < 8; i++) {
    //String oName=String("lightBulb") + i;
    new SpanAccessory();
    //new DEV_Identify(oName.c_str(), "MORDOMUS", "000000", "OF8Wd", "0.1", 0); // CHANGED! The number of blinks is now set to zero
    new DEV_Identify("lightbulb", "MORDOMUS", "000000", "OF8Wd", "0.1", 0); // CHANGED! The number of blinks is now set to zero
    OF8WdOutput* output = new OF8WdOutput(OF8Wd_ADDRESS + i);
    output->setupdate_calback([&](byte address, bool newPowerState) {
      pccwdController.setOnSate(address,  newPowerState);
    });

  }

  for (int i = 0; i < 10; i++) {
    //String sName=String("switch") + i;
    new SpanAccessory();
  //  new DEV_Identify(sName.c_str(), "MORDOMUS", "000000", "IN10wd", "0.1", 0); // CHANGED! The number of blinks is now set to zero
    new DEV_Identify("switch", "MORDOMUS", "000000", "IN10wd", "0.1", 0);
    switches[i] = new IN10wDStatelessProgrammableSwitch(IN10Wd_ADDRESS + i);            // create an on/off LED attached to pin 16
  }

  pccwdController.setProgrammableSwitchEvent_callback([&](uint8_t id, PressType pressType) {
    uint8_t index = id - IN10Wd_ADDRESS;
    LOG_D("switch pressed at address: %u to type: %u",index,(int)pressType);
    switches[index]->Pressed((int)pressType);
  });

} // end of setup()

//////////////////////////////////////

void loop() {

  homeSpan.poll();
  pccwdController.loop();

} // end of loop()
