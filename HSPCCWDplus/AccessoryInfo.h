#ifndef ACCESSORY_INFO_H
#define ACCESSORY_INFO_H

#include "HomeSpan.h"

// Simple AccessoryInformation service without Identify callback
// Use for accessories like StatelessProgrammableSwitch that don't need identify
struct AccessoryInfo : Service::AccessoryInformation {

  AccessoryInfo(const char *name, const char *manu, const char *sn, const char *model, const char *version)
    : Service::AccessoryInformation() {

    new Characteristic::Name(name);
    new Characteristic::Manufacturer(manu);
    new Characteristic::SerialNumber(sn);
    new Characteristic::Model(model);
    new Characteristic::FirmwareRevision(version);
    new Characteristic::Identify();  // Required but no update() override
  }
};

#endif
