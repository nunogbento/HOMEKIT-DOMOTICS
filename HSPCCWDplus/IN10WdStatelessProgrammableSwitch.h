
#include "PCCWDController.h"

struct IN10wDStatelessProgrammableSwitch : Service::StatelessProgrammableSwitch {       // Stateless Programmable Switch

  SpanCharacteristic *switchEvent;                  // reference to the ProgrammableSwitchEvent Characteristic

  IN10wDStatelessProgrammableSwitch(byte address) : Service::StatelessProgrammableSwitch() {

    switchEvent = new Characteristic::ProgrammableSwitchEvent(); // Programmable Switch Event Characteristic (will be set to SINGLE, DOUBLE or LONG press)
    // Note: ServiceLabelIndex removed - only needed for multiple switches per accessory

    

  } // end constructor

  // We do NOT need to implement an update() method or a loop() method - just the button() method:

  void Pressed(int pressType)  {
    switchEvent->setVal(pressType);                // set the value of the switchEvent Characteristic
  }

};
