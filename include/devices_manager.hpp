#ifndef _DEVICES_MANAGER_HPP_
#define _DEVICES_MANAGER_HPP_

#include <Arduino.h>
#include <ArduinoJson.h>

bool setupSimpleDevice(String deviceName, int GPIOpin);

void controlSimpleDevice(String deviceName, int GPIOpin, bool state);

bool removeSimpleDevice(String deviceName, int GPIOpin);

bool getDeviceList(JsonDocument& deviceList);

// Device Manager produce a Queue notifying device status change, so who ever need to access the Queue need to 
// call this function to increase the receiver count.
void deviceChangedQueueReceiverCountInc();

#endif // _DEVICES_MANAGER_HPP_