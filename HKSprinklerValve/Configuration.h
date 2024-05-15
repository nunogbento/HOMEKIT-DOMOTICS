#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_




#define ACCESSORY_NAME "SPINKLERS"

#define VALVE_1_TYPE 1
#define VALVE_2_TYPE 1
#define VALVE_3_TYPE 1
#define VALVE_4_TYPE 1

#define VALVE_1_PIN 13
#define VALVE_2_PIN 16
#define VALVE_3_PIN 14
#define VALVE_4_PIN 12

#define SDA_PIN 4
#define SCL_PIN 5

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
