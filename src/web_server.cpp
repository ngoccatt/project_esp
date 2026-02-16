#include "web_server.hpp"
#include "littleFS_manager.hpp"
#include "global.hpp"
#include "wifi_manager.hpp"
#include "devices_manager.hpp"
#include "task_core_iot.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data)
{
    if (ws.count() > 0)
    {
        ws.textAll(data); // Gửi đến tất cả client đang kết nối
        Serial.println("Data send to WebSocket: " + data);
    }
    else
    {
        Serial.println("No client, sent " + data);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message;
            message += String((char *)data).substring(0, len);
            requestProcessing(message);
            Serial.printf("Received Data: %s", message.c_str());
        }
    }
}

// Process incoming requests from WebSocket
void requestProcessing(String data)
{
    static JsonDocument recvDoc;
    // Parse JSON data
    DeserializationError error = deserializeJson(recvDoc, data);
    if (error)
    {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        return;
    }

    // First, get the page of request
    String page = recvDoc["page"];
    String type = recvDoc["type"];
    if (type == "request") {
        // for "setting" page, we update the WIFI and CORE IOT settings
        if (page == "setting")
        {
            Serial.println("HANDLING SETTINGS");
            String wifi_ssid = recvDoc["value"]["ssid"].as<String>();
            String wifi_pass = recvDoc["value"]["password"].as<String>();
            String token = recvDoc["value"]["token"].as<String>();
            String server = recvDoc["value"]["server"].as<String>();
            String port = recvDoc["value"]["port"].as<String>();

            // hidden mechanism to reset back to AP mode: input a space in SSID field
            // trim function will remove leading/trailing spaces, lead to empty string ""
            wifi_ssid.trim();
            wifi_pass.trim();
            token.trim();
            server.trim();
            port.trim();

            updateSTAConfig(wifi_ssid, wifi_pass);
            updateCoreIoTConfig(server, token, port);

            JsonDocument infoDoc;
            loadInfoFile(infoDoc);
            infoDoc["setting"]["WIFI_SSID"] = wifi_ssid;
            infoDoc["setting"]["WIFI_PASS"] = wifi_pass;
            infoDoc["setting"]["CORE_IOT_TOKEN"] = token;
            infoDoc["setting"]["CORE_IOT_SERVER"] = server;
            infoDoc["setting"]["CORE_IOT_PORT"] = port;
            saveInfoFile(infoDoc);
        }
        // for "device" page, we handle device create/control/remove actions
        else if (page == "device")
        {
            Serial.println("HANDLING DEVICES");
            String device_name = recvDoc["value"]["name"].as<String>();
            int device_gpio = recvDoc["value"]["gpio"].as<int>();
            bool status = recvDoc["value"]["status"].as<bool>();
            String action = recvDoc["value"]["action"].as<String>();
            if (action == "create")
            {
                setupSimpleDevice(device_name, device_gpio);
            }
            else if (action == "control")
            {
                controlSimpleDevice(device_name, device_gpio, status);
            }
            else if (action == "remove")
            {
                removeSimpleDevice(device_name, device_gpio);
            }
        }
        else
        {
            Serial.println("Unknown page request: " + page);
        }
    } 
    else if (type == "query")
    {
        JsonDocument sendDoc;
        String sendData;
        if (page == "setting")
        {
            sendDoc["page"] = "setting";
            sendDoc["value"]["ssid"] = getCurrentSSID();
            sendDoc["value"]["password"] = getCurrentPASS();
            sendDoc["value"]["token"] = coreIotGetCurrentToken();
            sendDoc["value"]["server"] = coreIotGetCurrentServer();
            sendDoc["value"]["port"] = coreIotGetCurrentPort();
            serializeJson(sendDoc, sendData);
            Webserver_sendata(sendData);
        }
        else if (page == "device")
        {
            // Extract device list from Devices manager
            JsonDocument tempDoc;
            if (getDeviceList(tempDoc)) {
                sendDoc["page"] = "device";
                sendDoc["value"] = tempDoc.as<JsonObject>();
            }
            serializeJson(sendDoc, sendData);
            Webserver_sendata(sendData);
        }
        else
        {
            Serial.println("Unknown page query: " + page);
        }
    }
    
}

void connnectWSV()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
    server.begin();
    webserver_isrunning = true;
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (!webserver_isrunning)
    {
        connnectWSV();
    }
    else
    {
    }
}

void Webserver_update()
{
    static String data;
    // Allocate the JSON document
    static JsonDocument sendDoc;
    if (!webserver_isrunning) return;
    else
    {
        sendDoc["page"] = "home";
        sendDoc["value"].to<JsonObject>();
        sendDoc["value"]["temperature"] = temperature;
        sendDoc["value"]["humidity"] = humidity;
        serializeJson(sendDoc, data);
        Webserver_sendata(data);
    }
}

void task_run_WebServer(void *pvParameters) {
    while(true) 
    {
        Webserver_reconnect();
        Webserver_update();
        vTaskDelay(2000);
    }
}
