#include "ledblinky.hpp"
#include "Arduino.h"

void taskLEDblinky(void *pvParameters) {
    pinMode(LED_STATUS_PIN, OUTPUT);
    static bool ledState = false;
    
    while(true)
    {
        digitalWrite(LED_STATUS_PIN, ledState ? HIGH : LOW);
        ledState = !ledState;
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
    }
}