
#include "task_core_iot.h"
#include "global.hpp"
#include "devices_manager.hpp"

String CORE_IOT_TOKEN = "";
String CORE_IOT_SERVER = "";
String CORE_IOT_PORT = "";

// Maximum size packets will ever be sent or received by the underlying MQTT client,
// if the size is to small messages might not be sent or received messages will be discarded
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 256U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 256U;

// constexpr const char RPC_JSON_METHOD[] = "example_json";
// constexpr const char RPC_TEMPERATURE_METHOD[] = "example_set_temperature";
// constexpr const char RPC_SWITCH_METHOD[] = "example_set_switch";
// constexpr const char RPC_TEMPERATURE_KEY[] = "temp";
// constexpr const char RPC_SWITCH_KEY[] = "switch";

constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 3U;
constexpr uint8_t MAX_RPC_RESPONSE = 5U;

// Initialize underlying client, used to establish a connection
WiFiClient wifiClient;
// Initalize the Mqtt client instance
Arduino_MQTT_Client mqttClient(wifiClient);
// Initialize used apis
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> rpc;
const std::array<IAPI_Implementation*, 1U> apis = {
    &rpc
};
// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE, Default_Max_Stack_Size, apis);

// Statuses for subscribing to rpc
bool subscribed = false;
bool wifi_connected = false;
bool reConnecteRequired = false;

/// @brief Processes function for RPC call "setValueLed1"
/// JsonVariantConst is a JSON variant, that can be queried using operator[]
/// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
/// @param data Data containing the rpc data that was called and its current value. 
/// Expect a JSON in the format {"name": deviceName, "gpio": GPIOpin, "status": true/false}
/// @param response Data containgin the response value, any number, string or json, that should be sent to the cloud. Useful for getMethods
void setValueLed(const JsonVariantConst &data, JsonDocument &response) {

    Serial.println("Received Switch state: " + data.as<String>());
    controlSimpleDevice(data["name"].as<String>(), data["gpio"].as<int>(), data["status"].as<bool>());
}

void coreIotSendData(String mode, String key, String data)
{
    if (mode == "attribute")
    {
        tb.sendAttributeData(key.c_str(), data);
    }
    else if (mode == "telemetry")
    {
        float value = data.toFloat();
        tb.sendTelemetryData(key.c_str(), value);
    }
    else
    {
        // handle unknown mode
    }
}

bool updateCoreIoTConfig(String server, String token, String port)
{
    if (server.length() == 0 || token.length() == 0 || port.length() == 0)
    {
        return false;
    }
    else
    {
        CORE_IOT_SERVER = server;
        CORE_IOT_TOKEN = token;
        CORE_IOT_PORT = port;
        reConnecteRequired = true;
        return true;
    }
}

String coreIotGetCurrentToken()
{
    return CORE_IOT_TOKEN;
}

String coreIotGetCurrentServer()
{
    return CORE_IOT_SERVER;
}

String coreIotGetCurrentPort()
{
    return CORE_IOT_PORT;
}

void coreIotReconnect()
{
    if (!tb.connected() || reConnecteRequired)
    {
        Serial.println("Attemp to connect, using server: " + CORE_IOT_SERVER + " token: " + CORE_IOT_TOKEN + " port: " + CORE_IOT_PORT);
        if (!tb.connect(CORE_IOT_SERVER.c_str(), CORE_IOT_TOKEN.c_str(), CORE_IOT_PORT.toInt()))
        {
            Serial.println("Failed to connect core IoT");
            return;
        }

        tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

        if (!subscribed)
        {
            Serial.println("Subscribing for RPC...");
            const std::array<RPC_Callback, MAX_RPC_SUBSCRIPTIONS> callbacks = {
                RPC_Callback{"setValueLed", setValueLed}
            };

            subscribed = rpc.RPC_Subscribe(callbacks.begin(), callbacks.end());
        }

        tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
        reConnecteRequired = false;
    }
    return;
}

void coreIotUploadData()
{
    static int delayedLoop = 1;
    static float temperature_l = 0.0;
    static float humidity_l = 0.0;
    static bool deviceChanged = false;
    // We do not want to upload data in every loop, so we can use a simple counter to create a delay
    if (delayedLoop > 0)
    {
        delayedLoop--;
    }
    else
    {
        // Temperature and humidity shall be send cylicly, so we can wait for a delay.
        xQueueReceive(xTemperatureQueue, &temperature_l, 5 / portTICK_PERIOD_MS);
        xQueueReceive(xHumidityQueue, &humidity_l, 5 / portTICK_PERIOD_MS);

        coreIotSendData("telemetry", "temperature", String(temperature_l));
        coreIotSendData("telemetry", "humidity", String(humidity_l));
        delayedLoop = 1;
    }
    xQueueReceive(xDeviceChangedQueue, &deviceChanged, 5 / portTICK_PERIOD_MS);
    if (deviceChanged) {
        JsonDocument deviceDoc;
        // query the device status and send it to coreiot.
        if (getDeviceList(deviceDoc)) {
            for (JsonPair kv : deviceDoc.as<JsonObject>()) {
                String name = kv.key().c_str();
                bool status = kv.value().as<JsonObject>()["status"].as<bool>();
                tb.sendAttributeData(name.c_str(), status ? "true" : "false");
                Serial.println("Device " + name + " changed, new status: " + (status ? "true" : "false"));
            }
        }
        deviceChanged = false;
    }
}

void taskCoreIot(void *pvParameters)
{
    wifi_connected = false;
    while (true)
    {
        if (wifi_connected)
        {
            /// create a connection to the ThingsBoard server if there is none yet or the connection was disrupted
            coreIotReconnect();

            if (tb.connected())
            {
                coreIotUploadData();
                // This function basically process all the requests received from coreiot, so if we use too large delays, it also cause higher delay in received RPC calls from buttons.
                tb.loop();
            }
        }
        else if (!wifi_connected && xBinarySemaphoreInternet != NULL)
        {
            // Wait until we are connected to the internet before trying to establish a connection to ThingsBoard
            if (xSemaphoreTake(xBinarySemaphoreInternet, 1000 / portTICK_PERIOD_MS) == pdTRUE)
            {
                Serial.println("Internet connection established, starting Core IoT task");
                wifi_connected = true;
            }
            else
            {
                Serial.println("Failed to take semaphore for internet connection");
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        // this Task delay is important to prevent the task from consuming all CPU time while waiting for the semaphore. Without this delay, the background task won't be able to get the cpu and reset the watchdog, which will lead to a crash of the system.
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}