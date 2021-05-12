#include <WiFiManager.h>

/*
 * wifi_info.h
 *
 *  Created on: 2020-05-15
 *      Author: Mixiaoxiao (Wang Bin)
 */

#ifndef WIFI_INFO_H_
#define WIFI_INFO_H_

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif


void wifi_connect() {
	//Local intialization. Once its business is done, there is no need to keep it around  
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect(chipId);
}

#endif /* WIFI_INFO_H_ */
