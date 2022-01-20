#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_
#include <Arduino.h>

#define ACCESSORY_NAME "Bia Feeder"

#define FEEDERSERVOPIN 13//7
#define FEEDERLDRPIN 12//6

#define SDA_PIN 4
#define SCL_PIN 5

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
