#include "wifi_manager.hpp"
#include "global.hpp"
#include "littleFS_manager.hpp"

String WIFI_SSID = "";
String WIFI_PASS = "";
String currentIP = "";

bool reConnectRequired = false;

const int NUM_OF_TRIALS_FOR_STA = 5;

void startAP()
{
    if (WiFi.getMode() == WIFI_MODE_AP)
    {
        // Already in AP mode, nothing to do
        return;
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    currentIP = WiFi.softAPIP().toString();
}

String getCurrentIP()
{
    return currentIP;
}

String getCurrentSSID()
{
    return WIFI_SSID;
}

String getCurrentPASS()
{
    return WIFI_PASS;
}

bool updateSTAConfig(String ssid, String pass)
{
    reConnectRequired = true;
    if (ssid.isEmpty())
    {
        WIFI_SSID = "";
        WIFI_PASS = "";
        return false;
    }
    else
    {
        WIFI_SSID = ssid;
        WIFI_PASS = pass;
        return true;
    }
}   

void startSTA()
{
    // if no WIFI configured, this task is invalid
    int trials = 0;
    if (WIFI_SSID.isEmpty())
    {
        return;
    }

    WiFi.mode(WIFI_STA);
    Serial.println("Connecting to WiFi SSID: " + WIFI_SSID + " With Pass: " + WIFI_PASS);

    // Try to connect without password
    if (WIFI_PASS.isEmpty())
    {
        WiFi.begin(WIFI_SSID.c_str());
    }
    // connect with password
    else
    {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    TickType_t startTick = xTaskGetTickCount();
    while (WiFi.status() != WL_CONNECTED)
    {
        if ((xTaskGetTickCount() - startTick) > (1000 / portTICK_PERIOD_MS))
        {
            trials++;
            startTick = xTaskGetTickCount();
        }
        if (trials > 5)
        {
            Serial.println("Failed to connect to WiFi after 5 trials. Stopping STA task.");
            return;
        }
    }

    Serial.println("Connected successfully to WiFi network: " + WIFI_SSID);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    currentIP = WiFi.localIP().toString();
    // Give a semaphore here
    xSemaphoreGive(xBinarySemaphoreInternet);
}

bool Wifi_reconnect()
{
    // Wifi.status is for STA mode only.
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        return true;
    }
    startSTA();
    return false;
    
}

void task_run_WiFiManager(void *pvParameters)
{
    // Start AP at first.
    int staTrials = 0;
    startAP();
    while (true)
    {
        // if 10 trials and no connection, revert back to AP mode.
        // reset SSID to "" will prevent STA function to run.
        if (reConnectRequired) staTrials = 0;
        if (WiFi.status() != WL_CONNECTED || reConnectRequired)
        {
            Serial.println("WiFi STA reconnect trial: " + String(staTrials));
            staTrials++;
            if (staTrials > NUM_OF_TRIALS_FOR_STA) {
                staTrials = 0;
                updateSTAConfig("", "");
                startAP();
            }
            startSTA();
            reConnectRequired = false;
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
