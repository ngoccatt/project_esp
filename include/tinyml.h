#ifndef __TINY_ML__
#define __TINY_ML__

#include <Arduino.h>

#ifdef ENABLE_TINYML
#include "dht_anomaly_model.h"

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

void setupTinyML();
void tiny_ml_task(void *pvParameters);

// TinyML produce a Queue notifying anomaly detection results, so who ever need to access the Queue need to 
// call this function to increase the receiver count.
void tinyMLQueueReceiverCountInc();

#endif // ENABLE_TINYML
#endif // __TINY_ML__