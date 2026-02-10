#ifndef _WEB_SERVER_
#define _WEB_SERVER_

#include <ESPAsyncWebServer.h>

#include <AsyncTCP.h>
#include <ArduinoJson.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

void Webserver_stop();
void Webserver_reconnect();
void Webserver_sendata(String data);
void requestProcessing(String data);
void Webserver_update();

void task_run_WebServer(void *pvParameters);

#endif // _WEB_SERVER_