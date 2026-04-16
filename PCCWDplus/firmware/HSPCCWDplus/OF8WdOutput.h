
#include "PCCWDController.h"

typedef std::function<void(byte address, bool newPowerState)> Update_calback;

struct OF8WdOutput : Service::LightBulb {               // ON/OFF LED

  byte _address;                                       // pin number defined for this LED
  SpanCharacteristic *power;                        // reference to the On Characteristic

  Update_calback _update_calback;

  OF8WdOutput(byte address) : Service::LightBulb() {      // constructor() method
    power = new Characteristic::On();
    this->_address = address;
  } // end constructor

  void setupdate_calback(Update_calback callback) {
    _update_calback = callback;
  }

  boolean update() {                             // update() method
    if (_update_calback)
      _update_calback(_address, power->getNewVal());
    return (true);                              // return true
  } // update
};
