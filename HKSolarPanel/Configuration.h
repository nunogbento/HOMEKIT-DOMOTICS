#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_


#define ACCESSORY_NAME "SolarPanel"
#define ONE_WIRE_BUS 14
#define MEASUREMENT_INTERVAL 100000UL
#define SAMPLE_INTERVAL 500UL//2 * 60 * 1000UL
#define SAMPLE_BUFFER_SIZE 200
#define MQTT_SERVER "192.168.1.109"
#define MQTT_USER  ""
#define MQTT_PASS  ""

#define LOG_TOPIC  "sensor/env/update"

#define SDA_PIN 5
#define SCL_PIN 4

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__)


#endif
