#include "temp_humid_mon.hpp"
#include "DHT20.h"

#define I2C_SDA 11
#define I2C_SCL 12

void taskMonitorTempHumid(void *pvParameters) {
    DHT20 dht20(&Wire);
    int i = 0;
    char msgBuf[256];
    Wire.begin(I2C_SDA, I2C_SCL);
    dht20.begin();
    bool checkConnection_l = false;
    bool connected_l = true;
    float temperature_l = 0.0;
    float humidity_l = 0.0;
    while (true) 
    {
        checkConnection_l = dht20.isConnected();
        if (!checkConnection_l) 
        {
            if (checkConnection_l != connected_l){
                Serial.println("DHT20 is not connected, using dummy value");
                connected_l = false;
            }
            temperature_l = i;
            humidity_l = (100 - i);
            i = (i + 1) % 100;
        }
        else 
        {
            connected_l = true;
            dht20.read();
            temperature_l = dht20.getTemperature();
            humidity_l = dht20.getHumidity();

            // Check if any reads failed then use dummy values
            if (isnan(temperature) || isnan(humidity)) {
                Serial.println("Failed to read from DHT sensor!");
        }
        }
        
        temperature = temperature_l;
        humidity = humidity_l;
        
        // sprintf(msgBuf, "Temp: %.2f - Humid: %.2f", temperature, humidity);
        // Serial.println(msgBuf);

        // dht library require each data reading must >= 1s
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}