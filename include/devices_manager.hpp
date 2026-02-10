#ifndef _DEVICES_MANAGER_HPP_
#define _DEVICES_MANAGER_HPP_

#include <Arduino.h>
#include <ArduinoJson.h>

bool setupSimpleDevice(String deviceName, int GPIOpin);

void controlSimpleDevice(String deviceName, int GPIOpin, bool state);

bool removeSimpleDevice(String deviceName, int GPIOpin);

bool getDeviceList(JsonDocument& deviceList);

#endif // _DEVICES_MANAGER_HPP_