#include <Adafruit_NeoPixel.h>
#include "neopixel_led.hpp"
#include "global.hpp"

void taskNeoLED(void *pvParameters) {
    Adafruit_NeoPixel strip(LED_COUNT, RBG_LED_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(50);

    int ledState = 0;
    int statusLoop = 0;

    float temperature_l = 0.0;
    float humidity_l = 0.0;

    while(1) 
    {
      xQueueReceive(xTemperatureQueue, &temperature_l, 5 / portTICK_PERIOD_MS);
      xQueueReceive(xHumidityQueue, &humidity_l, 5 / portTICK_PERIOD_MS);
      // incase the queue does not have data, call to QueueReceive will return pdFalse after timeout,
      // we assumed that old data is retained when timeout expired.
      if (ledState == 0) 
      {
        switch(statusLoop) 
        {
          case 0:
            strip.setPixelColor(0, strip.Color(int(temperature_l / 100 * 255) , 0, 0));
            break;
          // humidity
          case 1:
            strip.setPixelColor(0, strip.Color(0, 0, int(humidity_l / 100 * 255)));
            break;
        }
        statusLoop = (statusLoop + 1) % 2;
      }
      else
      {
        // turn off led
        strip.setPixelColor(0, strip.Color(0, 0, 0));
      }
      strip.show();
      ledState = (1 - ledState);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}