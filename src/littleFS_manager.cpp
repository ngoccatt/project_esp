#include "littleFS_manager.hpp"
#include "wifi_manager.hpp"


/*
File format:
{
  "setting" : {
    "WIFI_SSID": "ssid",
    "WIFI_PASS": "pass",
    "CORE_IOT_TOKEN": "token",
    "CORE_IOT_SERVER": "server",
    "CORE_IOT_PORT": "port"
    },
  "device": {
    "deviceName1": {"gpio": GPIOpin, "status": false},
    "deviceName2": {"gpio": GPIOpin, "status": false},
    ...
  },
}
*/

bool initializeLittleFS()
{
    if (!LittleFS.begin(true))
    {
      Serial.println("[ERROR] Cannot initialize Little FS");
      return false;
    }
  return true;
}

bool loadInfoFile(JsonDocument& doc)
{
  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    Serial.println(F("Failed to open info.dat"));
    return false;
  }
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.println("deserializeJson() failed: " + String(error.c_str()));
    return false;
  }
  file.close();
  serializeJson(doc, Serial);
  return true;
}

bool deleteInfoFile()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
    return true;
  }
  else
  {
    return false;
  }
  ESP.restart();
}

bool saveInfoFile(JsonDocument& doc)
{
  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
    return true;
  }
  else
  {
    Serial.println("Unable to save the configuration.");
    return false;
  }
};