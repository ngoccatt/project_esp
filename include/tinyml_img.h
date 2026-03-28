#ifndef __TINYML_IMG_H__
#define __TINYML_IMG_H__

#include <Arduino.h>

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"


void setupTinyMLForImage(void* pvParameter);
// Feed a pre-processed INT8 image tensor into the image classification model
// and run inference. pixelData must be a flat array of numValues int8_t values
// in [H][W][C] order, quantized as (uint8_channel - 128)  (scale=1/128, zp=0),
// which is equivalent to MobileNet float range [-1.0, +1.0].
void tinyMLRunImageInference(void* pvParameter);

#endif // __TINYML_IMG_H__