#ifndef __TASK_CORE_IOT_H__
#define __TASK_CORE_IOT_H__

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>

void coreIotSendData(String mode, String feed, String data);

void coreIotReconnect();

bool updateCoreIoTConfig(String server, String token, String port);

String coreIotGetCurrentToken();
String coreIotGetCurrentServer();
String coreIotGetCurrentPort();

void coreIotUploadData();

void taskCoreIot(void *pvParameters);

#endif // __TASK_CORE_IOT_H__