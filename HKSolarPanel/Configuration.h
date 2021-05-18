#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_


#define ACCESSORY_NAME "SolarPanel"
#define ONE_WIRE_BUS 14
#define MQTT_SERVER "192.168.1.109"


#define TELEMETRYTOPIC(chipid)  PSTR(chipid)+"/from"

#define SDA_PIN 5
#define SCL_PIN 4

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)


#endif
