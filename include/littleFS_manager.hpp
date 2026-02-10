#ifndef _LITTLEFS_MANAGER_
#define _LITTLEFS_MANAGER_

#include "LittleFS.h"
#include "Arduino.h"
#include <ArduinoJson.h>

bool initializeLittleFS(bool check);

bool loadInfoFile(JsonDocument& doc);

bool deleteInfoFile();

bool saveInfoFile(JsonDocument& doc);

#endif //_LITTLEFS_MANAGER_