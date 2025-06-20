#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_


#define ACCESSORY_NAME "LS Light"
#define IR_LED_PIN 21

#define L1PIN 6
#define L2PIN 4

#define cw1_LedPin 6
#define ww1_LedPin 5
#define cw2_LedPin 4
#define ww2_LedPin 2

#define WHITE_LedPin 2
#define RED_LedPin 4
#define GREEN_LedPin 5
#define BLUE_LedPin 6

#define SDA_PIN 10
#define SCL_PIN 3

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)

#endif
