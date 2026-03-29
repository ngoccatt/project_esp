#include "global.hpp"
#include "ei_inference.hpp"

#ifdef ENABLE_EI_CLASSIFIER

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include "image_concat.hpp"
#include "esp_task_wdt.h"


#define kBatteryIndex 0
#define kBiologicalIndex 1
#define kCardboardIndex 2
#define kMetalIndex 3
#define kPaperIndex 4

static int eiImageDetectionReceiverCount = 0;

static float max_score = 0.0;
static String max_label = "";
static int infer_time = 0;

static float *pixelBuf = nullptr;
static size_t numValues = 0;

void eiImageDetectionReadyQueueReceiverCountInc() {
    eiImageDetectionReceiverCount++;
}

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, pixelBuf + offset, length * sizeof(float));
  return 0;
}

void eiSetupForImage(void *pvParameter)
{
    ei_printf("Edge Impulse standalone inferencing (Espressif ESP32)\n");

    // run_classifier_init();

    ei_printf("Initialization completed\n");
}



void eiRunImageInference(void *pvParameter)
{
    xEiImageDetectionQueue = xQueueCreate(10, sizeof(bool));

    // Inference can take 20-40s on float32 model 
    // this function help remove this task from TWDT (watchdog timer)
    // also, pin this task to core 1 to avoid blocking system tasks on core 0
    esp_task_wdt_delete(NULL);

    // delay for 10s (first setup)
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ei_impulse_result_t result = { nullptr };
    

    signal_t features_signal;
    features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    features_signal.get_data = &raw_feature_get_data;

    EI_IMPULSE_ERROR res;

    while (1)
    {
        
        // Point directly at the static s_modelInput buffer in image_concat — zero copies.
        if (!getModelInputBufferFloat(pixelBuf, numValues))
        {
            Serial.println("[Step 5] No new image available, skipping inference.");
        }
        else
        {
            Serial.printf("[Step 5] eiRunImageInference called. numValues=%d\n", (int)numValues);

            
            if (numValues != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
            {
                Serial.printf("Model input size mismatch. expected=%d got=%d\n",
                              EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, (int)numValues);
                continue; // skip inference
            }

            // invoke the impulse
            res = run_classifier(&features_signal, &result, false /* debug */);
            if (res != EI_IMPULSE_OK) {
                ei_printf("ERR: Failed to run classifier (%d)\n", res);
                continue; // skip output reading — impulse state is invalid
            }

            infer_time = result.timing.classification;

            // Print how long it took to perform inference
            ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
                        result.timing.dsp,
                        result.timing.classification,
                        result.timing.anomaly);

            // Process the inference results.

            // get max score and corresponding label
            max_score = result.classification[0].value;
            max_label = ei_classifier_inferencing_categories[0];
            for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
            {
                if (result.classification[i].value > max_score)
                {
                    max_score = result.classification[i].value;
                    max_label = ei_classifier_inferencing_categories[i];
                }
            }

            Serial.printf("Ei Inference results: Battery=%.3f, Biological=%.3f, Cardboard=%.3f, Metal=%.3f, Paper=%.3f\n",
                          result.classification[kBatteryIndex].value, result.classification[kBiologicalIndex].value,
                          result.classification[kCardboardIndex].value, result.classification[kMetalIndex].value,
                          result.classification[kPaperIndex].value);

            Serial.printf("Ei Max label: %s, Max score: %.3f\n", max_label.c_str(), max_score);

            bool ready = true;

            for(int i = 0; i < eiImageDetectionReceiverCount; i++) {
                xQueueSend(xEiImageDetectionQueue, &ready, 10 / portTICK_PERIOD_MS);
            }
            
            // delay for 10s (since the inference may be heavy)
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void eiGetInferenceResult(String& dLabel, float& dScore, int& dTimeMs) {
    dLabel = max_label;
    dScore = max_score;
    dTimeMs = infer_time;
}

#endif // ENABLE_EI_CLASSIFIER