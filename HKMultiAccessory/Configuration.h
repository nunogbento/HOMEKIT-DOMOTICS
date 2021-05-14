#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_


//#define _RGB_
//#define _RGBW_
//#define _DIMMER_
#define _DUAL_
//#define _AC_
//#define _TH_
#define ACCESSORY_NAME "Multi Accessory"
#define IR_LED_PIN 10

#define L1PIN 13
#define L2PIN 14

#define WHITE_LedPin 13
#define RED_LedPin 16
#define GREEN_LedPin 14
#define BLUE_LedPin 12

#define SDA_PIN 4
#define SCL_PIN 5

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
