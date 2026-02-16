#include <Arduino.h>
#include "temp_humid_mon.hpp"
#include "neopixel_led.hpp"
#include "wifi_manager.hpp"
#include "web_server.hpp"
#include "littleFS_manager.hpp"
#include "ledblinky.hpp"
#include "devices_manager.hpp"
#include "tinyml.h"
#include "task_core_iot.h"
#include "global.hpp"

// Little FS https://randomnerdtutorials.com/esp32-vs-code-platformio-littlefs/

void infoFileSetup()
{
  initializeLittleFS(false);
  JsonDocument infoDoc;
  if (loadInfoFile(infoDoc))
  {
    String wifi_ssid = infoDoc["setting"]["WIFI_SSID"].as<String>();
    String wifi_pass = infoDoc["setting"]["WIFI_PASS"].as<String>();
    String token = infoDoc["setting"]["CORE_IOT_TOKEN"].as<String>();
    String server = infoDoc["setting"]["CORE_IOT_SERVER"].as<String>();
    String port = infoDoc["setting"]["CORE_IOT_PORT"].as<String>();
    updateSTAConfig(wifi_ssid, wifi_pass);
    updateCoreIoTConfig(server, token, port);
    for (JsonPair kv : infoDoc["device"].as<JsonObject>())
    {
      String device_name = kv.key().c_str();
      int device_gpio = kv.value().as<JsonObject>()["gpio"].as<int>();
      setupSimpleDevice(device_name, device_gpio);
    }
  }
  else
  {
    // Info file is not present, then create all the items with "" to ensure no null read happened.
    Serial.println("Create info.dat file for the first time");
    infoDoc["setting"]["WIFI_SSID"] = "";
    infoDoc["setting"]["WIFI_PASS"] = "";
    infoDoc["setting"]["CORE_IOT_TOKEN"] = "";
    infoDoc["setting"]["CORE_IOT_SERVER"] = "";
    infoDoc["setting"]["CORE_IOT_PORT"] = "";
    infoDoc["device"].to<JsonObject>();
    saveInfoFile(infoDoc);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  initializeLittleFS(false);
  infoFileSetup();
  // prepare semaphore initialization.
  xBinarySemaphoreInternet = xSemaphoreCreateBinary();

  // Tasks creation
  xTaskCreate(taskLEDblinky, "LED Blinky", 1024, NULL, 1, NULL);
  xTaskCreate(task_run_WiFiManager, "WiFi Manager", 4096, NULL, 3, NULL);
  xTaskCreate(taskMonitorTempHumid, "Temp_Humid", 4096, NULL, 3, NULL);
  xTaskCreate(taskNeoLED, "LED Blink", 2048, NULL, 2, NULL);
  xTaskCreate(task_run_WebServer, "Web Server", 1024*8, NULL, 3, NULL);
  xTaskCreate(tiny_ml_task, "TinyML Task", 4096*2, NULL, 3, NULL);
  xTaskCreate(taskCoreIot, "Core IoT", 4096*2, NULL, 3, NULL);
}

void loop() {
  // put your main code here, to run repeatedly:
}
