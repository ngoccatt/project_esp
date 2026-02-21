#include "ledblinky.hpp"
#include "Arduino.h"
#include "temp_humid_mon.hpp"
#include "global.hpp"

void taskLEDblinky(void *pvParameters) {
    static bool ledState = false;
    float temperature_l = 0.0;
    float unused_l = 0.0;
    int delay_time = 2000; // default delay time in milliseconds
    int polling_interval = 2000; // default polling interval in milliseconds
    int polling_counter = polling_interval / delay_time; // Initialize the counter based on the polling interval and delay time

    tempHumidMonQueueReceiverCountInc();
    pinMode(LED_STATUS_PIN, OUTPUT);

    while(true)
    {
        if (polling_counter == 0) {
            xQueueReceive(xTemperatureQueue, &temperature_l, 5 / portTICK_PERIOD_MS);
            xQueueReceive(xHumidityQueue, &unused_l, 5 / portTICK_PERIOD_MS);
            polling_counter = polling_interval / delay_time; // Reset the counter based on the polling interval
        } else {
            polling_counter--; // Decrement the counter on each loop iteration
        }
        
        digitalWrite(LED_STATUS_PIN, ledState ? HIGH : LOW);
        ledState = !ledState;
        if (temperature_l <= 20) {
            delay_time = 2000; // 2 seconds
        }
        else if (temperature_l > 20 && temperature_l < 25) {
            delay_time = 1500; // 1.5 seconds
        } else if (temperature_l >= 25 && temperature_l < 30) {
            delay_time = 1000; // 1 second
        } else if (temperature_l >= 30 && temperature_l < 35) {
            delay_time = 500; // 0.5 seconds
        } else if (temperature_l >= 35 && temperature_l < 40) {
            delay_time = 250; // 0.25 seconds
        } else {
            delay_time = 100; // 0.1 seconds
        }
        vTaskDelay(delay_time / portTICK_PERIOD_MS); // Delay for the calculated time
    }
}