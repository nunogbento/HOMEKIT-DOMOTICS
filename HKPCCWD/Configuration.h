#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_
#include <Arduino.h>

#define HOMEKIT_NO_LOG 0
#define HOMEKIT_LOG_LEVEL HOMEKIT_NO_LOG

#define ACCESSORY_NAME "PCCWD"

#define IN10Wd_ADDRESS 74
#define OF8Wd_ADDRESS 167


#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
