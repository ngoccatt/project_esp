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
extern QueueHandle_t xImageDetectionQueue;
extern QueueHandle_t xEiImageDetectionQueue;
extern QueueHandle_t xDeviceChangedQueue;

// --- Inference backend selection ---
// Switch backends by changing ONE line in platformio.ini:
//   default_envs = tinyml        →  injects -D ENABLE_TINYML
//   default_envs = ei_classifier →  injects -D ENABLE_EI_CLASSIFIER

#endif //_GLOBAL_HPP

