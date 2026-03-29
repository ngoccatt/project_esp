#ifndef _EI_INFERENCE_HPP_
#define _EI_INFERENCE_HPP_

#ifdef ENABLE_EI_CLASSIFIER

#include <Arduino.h>

void eiImageDetectionReadyQueueReceiverCountInc();

void eiSetupForImage(void *pvParameter);

void eiRunImageInference(void *pvParameter);

void eiGetInferenceResult(String& dLabel, float& dScore, int& dTimeMs);


#endif // ENABLE_EI_CLASSIFIER
#endif // _EI_INFERENCE_HPP_