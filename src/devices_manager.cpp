#include "devices_manager.hpp"
#include "littleFS_manager.hpp"
#include "global.hpp"

JsonDocument devicesDoc;
/*
format: 
{
    {"deviceName1": {"gpio": GPIOpin, "status": false}},
    {"deviceName2": {"gpio": GPIOpin, "status": false}},
}
*/ 

int deviceChangedQueueReceiverCount = 0;

void deviceChangedQueueReceiverCountInc() {
    deviceChangedQueueReceiverCount++;
}

bool setupSimpleDevice(String deviceName, int GPIOpin)
{
    // basically the same as deviceDoc.containsKey(deviceName)
    if (devicesDoc[deviceName].is<JsonObject>())
    {
        // Serial.println("Device " + deviceName + " is already set up.");
        return false;
    }
    devicesDoc[deviceName]["gpio"] = GPIOpin;
    devicesDoc[deviceName]["status"] = false;
    JsonDocument infoDoc;
    if (loadInfoFile(infoDoc))
    {
        infoDoc["device"][deviceName]["gpio"] = GPIOpin;
        infoDoc["device"][deviceName]["status"] = false;
        saveInfoFile(infoDoc);
    }
    pinMode(GPIOpin, OUTPUT);
    Serial.println("Device " + deviceName + " is set up on GPIO " + String(GPIOpin));
    return true;
}

bool getDeviceList(JsonDocument& deviceList)
{
    deviceList = devicesDoc;
    return true;
}

void controlSimpleDevice(String deviceName, int GPIOpin, bool state)
{
    // if device is not setup, we can't control it :)
    if (!devicesDoc[deviceName].is<JsonObject>())
    {
        // Serial.println("Device " + deviceName + " is not set up.");
        return;
    }
    digitalWrite(GPIOpin, state ? HIGH : LOW);
    devicesDoc[deviceName]["status"] = state;
    bool deviceChanged = true;
    for (int i = 0; i < deviceChangedQueueReceiverCount; i++) {
        xQueueSend(xDeviceChangedQueue, &deviceChanged,  5 / portTICK_PERIOD_MS);
    }
}

bool removeSimpleDevice(String deviceName, int GPIOpin)
{
    if (!devicesDoc[deviceName].is<JsonObject>())
    {
        // Serial.println("Device " + deviceName + " is not set up.");
        return false;
    }
    devicesDoc.remove(deviceName);
    JsonDocument infoDoc;
    if (loadInfoFile(infoDoc))
    {
        infoDoc["device"].remove(deviceName);
        saveInfoFile(infoDoc);
    }
    pinMode(GPIOpin, INPUT);
    // Serial.println("Device " + deviceName + " is removed from GPIO " + String(GPIOpin));
    return true;
}