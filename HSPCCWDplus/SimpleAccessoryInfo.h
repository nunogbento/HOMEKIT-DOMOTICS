#ifndef SIMPLE_ACCESSORY_INFO_H
#define SIMPLE_ACCESSORY_INFO_H

// Simple AccessoryInformation service without Identify callback
// Use for accessories like StatelessProgrammableSwitch that don't need identify
struct SimpleAccessoryInfo : Service::AccessoryInformation {

  SimpleAccessoryInfo(const char *name, const char *manu, const char *sn, const char *model, const char *version)
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
