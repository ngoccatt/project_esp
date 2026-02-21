#include <Adafruit_NeoPixel.h>
#include "neopixel_led.hpp"
#include "global.hpp"
#include "temp_humid_mon.hpp"
#include "wifi_manager.hpp"

void taskNeoLED(void *pvParameters) {
    tempHumidMonQueueReceiverCountInc();

    Adafruit_NeoPixel strip(LED_COUNT, RBG_LED_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(50);

    int ledState = 0;
    int statusLoop = 0;

    float unused_l = 0.0;
    float humidity_l = 0.0;

    int rssi = 0;
    int greenValue = 0;

    uint32_t humidColor = 0;

    while(1) 
    {
      xQueueReceive(xTemperatureQueue, &unused_l, 5 / portTICK_PERIOD_MS);
      xQueueReceive(xHumidityQueue, &humidity_l, 5 / portTICK_PERIOD_MS);
      // incase the queue does not have data, call to QueueReceive will return pdFalse after timeout,
      // we assumed that old data is retained when timeout expired.
      if (ledState == 0) 
      {
        switch(statusLoop) 
        {
          case 0:
            if (isWifiConnected()) {
              rssi = WifiStrength();
              // Map RSSI to a color gradient from light green (weak) to bright green (strong)
              greenValue = map(rssi, -100, 0, 50, 255); // Adjust the range as needed
              strip.setPixelColor(0, strip.Color(0, greenValue, 0));
              // Serial.println("WiFi Connected. RSSI: " + String(rssi) + " dBm, Green Value: " + String(greenValue));
            } else {
              strip.setPixelColor(0, strip.Color(int(255), 0, 0)); // Red for WiFi disconnected or AP mode is on.
            }
            break;
          // humidity
          case 1:
            // https://community.windy.com/topic/17430/humidity-color-scale
            // prepare the color
            if (humidity_l >= 0 && humidity_l < 12) {
              humidColor = strip.Color(240, 0, 0);
            } else if (humidity_l >= 12 && humidity_l < 22) {
              humidColor = strip.Color(200, 66, 13);
            } else if (humidity_l >= 22 && humidity_l < 32) {
              humidColor = strip.Color(194, 134, 62);
            } else if (humidity_l >= 32 && humidity_l < 42) {
              humidColor = strip.Color(105, 173, 56);
            } else if (humidity_l >= 42 && humidity_l <= 52) {
              humidColor = strip.Color(117, 203, 190);
            } else if (humidity_l >= 52 && humidity_l <= 62) {
              humidColor = strip.Color(56, 174, 173);
            } else if (humidity_l >= 62 && humidity_l <= 72) {
              humidColor = strip.Color(56, 157, 173);
            } else if (humidity_l >= 72 && humidity_l <= 82) {
              humidColor = strip.Color(15, 147, 167);
            } else if (humidity_l >= 82 && humidity_l <= 92) {
              humidColor = strip.Color(56, 132, 173);
            }else {
              humidColor = strip.Color(56, 70, 114); 
            }
            // set the color
            strip.setPixelColor(0, humidColor);
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