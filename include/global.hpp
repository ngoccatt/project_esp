#ifndef _GLOBAL_HPP
#define _GLOBAL_HPP

// FreeRTOS must be included before task.h and semphr.h
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <Arduino.h>

extern SemaphoreHandle_t xBinarySemaphoreInternet;
extern QueueHandle_t xTemperatureQueue;
extern QueueHandle_t xHumidityQueue;
extern QueueHandle_t xAnomalyQueue;
extern QueueHandle_t xDeviceChangedQueue;

#endif //_GLOBAL_HPP

