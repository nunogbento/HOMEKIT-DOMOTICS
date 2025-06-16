#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_


#define ACCESSORY_NAME "LVRL Light"
#define IR_LED_PIN 10

#define L1PIN 13
#define L2PIN 14

#define cw1_LedPin 13
#define ww1_LedPin 16
#define cw2_LedPin 14
#define ww2_LedPin 12

#define WHITE_LedPin 13
#define RED_LedPin 16
#define GREEN_LedPin 14
#define BLUE_LedPin 12

#define SDA_PIN 4
#define SCL_PIN 5

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
