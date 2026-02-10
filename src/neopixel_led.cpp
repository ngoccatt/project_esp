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
    while(1) 
    {
      if (ledState == 0) 
      {
        switch(statusLoop) 
        {
          // temperature
          case 0:
            strip.setPixelColor(0, strip.Color(int(temperature / 100 * 255) , 0, 0));
            break;
          // humidity
          case 1:
            strip.setPixelColor(0, strip.Color(0, 0, int(humidity / 100 * 255)));
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