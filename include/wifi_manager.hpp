#ifndef _WIFI_MANAGER_HPP_
#define _WIFI_MANAGER_HPP_

#include "WiFi.h"

void startAP();

void startSTA();

bool Wifi_reconnect();
bool updateSTAConfig(String ssid, String pass);
void task_run_WiFiManager(void *pvParameters);

bool isWifiConnected();
int WifiStrength();

String getCurrentIP();
String getCurrentSSID();
String getCurrentPASS();

#endif // _WIFI_MANAGER_HPP_