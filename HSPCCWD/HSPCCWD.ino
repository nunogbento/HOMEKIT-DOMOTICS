
#include "HomeSpan.h"
#include "PCCWDController.h"
#include "OF8WdOutput.h"
#include "IN10wDStatelessProgrammableSwitch.h"
#include "DEV_Identify.h"



IN10wDStatelessProgrammableSwitch* switches[10];

PCCWDController pccwdController(Serial1);

void setup() {

  Serial.begin(115200);
  Serial1.begin(14400,SERIAL,6,7);


  homeSpan.setStatusPin(15);
  homeSpan.setControlPin(0);
  homeSpan.setApSSID("PCCWDSETUP");
  homeSpan.setApPassword("");
  homespan.enableAutoStartAP();
  
  homeSpan.begin(Category::Bridges, "PCCWD","PCCWD","PCCWD Bridge");      

  
  new SpanAccessory();
  new DEV_Identify("PCCWD", "MORDOMUS", "000000", "PCCWD", "0.1", 3);


  for (int i = 0; i < 8; i++) {
  
    new SpanAccessory();  
    new DEV_Identify("lightbulb", "MORDOMUS", "000000", "OF8Wd", "0.1", 0); 
    OF8WdOutput* output = new OF8WdOutput(OF8Wd_ADDRESS + i);
    output->setupdate_calback([&](byte address, bool newPowerState) {
      pccwdController.setOnSate(address,  newPowerState);
    });

  }

  for (int i = 0; i < 10; i++) {
  
    new SpanAccessory();
    new DEV_Identify("switch", "MORDOMUS", "000000", "IN10wd", "0.1", 0);
    switches[i] = new IN10wDStatelessProgrammableSwitch(IN10Wd_ADDRESS + i);            
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
