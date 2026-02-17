#include "global.hpp"

SemaphoreHandle_t xBinarySemaphoreInternet = NULL;
QueueHandle_t xTemperatureQueue = NULL;
QueueHandle_t xHumidityQueue = NULL;
QueueHandle_t xAnomalyQueue = NULL;
QueueHandle_t xDeviceChangedQueue = xQueueCreate(10, sizeof(bool));