#ifndef _GLOBAL_HPP
#define _GLOBAL_HPP

// FreeRTOS must be included before task.h and semphr.h
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <Arduino.h>

extern float temperature;
extern float humidity;

extern SemaphoreHandle_t xBinarySemaphoreInternet;

#endif //_GLOBAL_HPP

