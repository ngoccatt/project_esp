#include "global.hpp"
#include "tinyml_img.h"
#include "garbage_detection_model.h"
#include "image_concat.hpp"

// Globals, used for compatibility with Arduino-style sketches.
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;

    // In order to use optimized tensorflow lite kernels, a signed int8_t quantized
    // model is preferred over the legacy unsigned model format. This means that
    // throughout this project, input images must be converted from unisgned to
    // signed format. The easiest and quickest way to convert from unsigned to
    // signed 8-bit integers is to subtract 128 from the unsigned value to get a
    // signed value.

    // An area of memory to use for input, output, and intermediate arrays.
    // 80KB internal arena is far too small for a ~547KB MobileNet model —
    // the first Conv2D layer alone produces 96×96×32 = 288KB of activations.
    // Allocate from PSRAM (8MB available on ESP32-N16R8) at runtime.
    // This TensorArenaSize is for storage of:
    // activation tensors, presistent buffers, scratch buffers, NOT THE MODEL.
    // the MODEL is stored in flash (ROM) actually, since it's a static const array.
    // --> So, the TensorArenaSize should be large enough for the biggest activation tensor?!
    constexpr int kTensorArenaSize = 512 * 1024; // 512KB from PSRAM
    static uint8_t *tensor_arena = nullptr;

} // namespace

#define kBatteryIndex 0
#define kBiologicalIndex 1
#define kCardboardIndex 2
#define kMetalIndex 3
#define kPaperIndex 4

static int imageDetectionReadyReceiverCount = 0;

static float max_score = 0.0;
static String max_label = "";
static int infer_time = 0;

void imageDetectionReadyQueueReceiverCountInc() {
    imageDetectionReadyReceiverCount++;
}

void setupTinyMLForImage(void *pvParameter)
{

    tflite::InitializeTarget();
    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    model = tflite::GetModel(garbage_detection_model);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        MicroPrintf(
            "Model provided is schema version %d not equal "
            "to supported version %d.",
            model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    // Pull in only the operation implementations we need.
    // This relies on a complete list of all the ops needed by this graph.

    // NOLINTNEXTLINE(runtime-global-variables)
    static tflite::MicroMutableOpResolver<8> micro_op_resolver;
    micro_op_resolver.AddAveragePool2D(tflite::Register_AVERAGE_POOL_2D_INT8());
    micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
    micro_op_resolver.AddDepthwiseConv2D(
        tflite::Register_DEPTHWISE_CONV_2D_INT8());
    micro_op_resolver.AddAdd(tflite::Register_ADD_INT8()); // MobileNetV2 residual additions
    micro_op_resolver.AddMean();                           // MobileNetV2 global average pooling
    micro_op_resolver.AddFullyConnected();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddSoftmax(tflite::Register_SOFTMAX_INT8());

    // Build an interpreter to run the model with.
    // NOLINTNEXTLINE(runtime-global-variables)

    // Allocate tensor arena from PSRAM — internal SRAM (~520KB total) cannot
    // hold 512KB contiguously alongside the rest of the firmware.
    if (tensor_arena == nullptr)
    {
        tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (tensor_arena == nullptr)
    {
        MicroPrintf("Failed to allocate tensor arena (%d bytes) from PSRAM", kTensorArenaSize);
        return;
    }

    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        MicroPrintf("AllocateTensors() failed — arena too small (%d bytes)", kTensorArenaSize);
        return;
    }
    Serial.printf("[TinyML] Arena used: %d / %d bytes\n",
                  (int)interpreter->arena_used_bytes(), kTensorArenaSize);
}


// -------------------------------------------------------------------
// Image inference stub.
// Replace this body with your actual image model setup and invoke.
// reference: https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/examples/person_detection/main_functions.cc
// -------------------------------------------------------------------
void tinyMLRunImageInference(void *pvParameter)
{

    // TODO: Set up a separate TFLite Micro interpreter for your image model.
    // Typical steps:
    //   1. Load image model from a header file (e.g. waste_classifier_model.h).
    //   2. Allocate a new tensor_arena sized for that model.
    //   3. Create MicroInterpreter and call AllocateTensors().
    //   4. memcpy(input->data.f, pixelData, numValues * sizeof(float));
    //   5. interpreter->Invoke();
    //   6. Read output->data.f[i] — argmax for multi-class, threshold for binary.
    xImageDetectionQueue = xQueueCreate(10, sizeof(bool));

    // delay for 10s (first setup)
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    static uint8_t *pixelBuf = nullptr;
    static size_t numValues = 0;

    // input must be placed outside of loop, or else the mcu is gonna crash? (core x panicked?)
    input = interpreter->input(0);

    while (1)
    {
        
        // Point directly at the static s_modelInput buffer in image_concat — zero copies.
        if (!getModelInputBuffer(pixelBuf, numValues))
        {
            Serial.println("[Step 5] No new image available, skipping inference.");
        }
        else
        {
            Serial.printf("[Step 5] tinyMLRunImageInference called. numValues=%d\n", (int)numValues);

            // s_modelInput is int8_t (pixel - 128), input tensor is also int8 — direct memcpy.
            memcpy(input->data.int8, pixelBuf, numValues * sizeof(int8_t));

            // Run the model on this input and make sure it succeeds.
            int64_t invoke_start_us = esp_timer_get_time();

            TfLiteStatus invoke_status = interpreter->Invoke();

            int64_t invoke_elapsed_us = esp_timer_get_time() - invoke_start_us;
            Serial.printf("[TinyML] Invoke() took %lld ms (%lld us)\n",
                          invoke_elapsed_us / 1000, invoke_elapsed_us);
            infer_time = (int)(invoke_elapsed_us / 1000);

            if (kTfLiteOk != invoke_status)
            {
                MicroPrintf("Invoke failed.");
                continue; // skip output reading — tensor state is invalid
            }
            
            output = interpreter->output(0);

            // Process the inference results.
            // Use int8 (signed) — uint8 would misinterpret negative quantized values.
            int8_t battery_score = output->data.int8[kBatteryIndex];
            int8_t biological_score = output->data.int8[kBiologicalIndex];
            int8_t cardboard_score = output->data.int8[kCardboardIndex];
            int8_t metal_score = output->data.int8[kMetalIndex];
            int8_t paper_score = output->data.int8[kPaperIndex];

            float final_scores[5];
            String labels[5] = {"Battery", "Biological", "Cardboard", "Metal", "Paper"};

            final_scores[kBatteryIndex] =
                (battery_score - output->params.zero_point) * output->params.scale;
            final_scores[kBiologicalIndex] =
                (biological_score - output->params.zero_point) * output->params.scale;
            final_scores[kCardboardIndex] =
                (cardboard_score - output->params.zero_point) * output->params.scale;
            final_scores[kMetalIndex] =
                (metal_score - output->params.zero_point) * output->params.scale;
            final_scores[kPaperIndex] =
                (paper_score - output->params.zero_point) * output->params.scale;

            // get max score and corresponding label
            max_score = final_scores[kBatteryIndex];
            max_label = labels[kBatteryIndex];
            for (int i = 1; i < 5; i++)
            {
                if (final_scores[i] > max_score)
                {
                    max_score = final_scores[i];
                    max_label = labels[i];
                }
            }

            Serial.printf("Inference results: Battery=%.3f, Biological=%.3f, Cardboard=%.3f, Metal=%.3f, Paper=%.3f\n",
                          final_scores[kBatteryIndex], final_scores[kBiologicalIndex],
                          final_scores[kCardboardIndex], final_scores[kMetalIndex],
                          final_scores[kPaperIndex]);

            Serial.printf("Max label: %s, Max score: %.3f\n", max_label.c_str(), max_score);

            bool ready = true;

            for(int i = 0; i < imageDetectionReadyReceiverCount; i++) {
                xQueueSend(xImageDetectionQueue, &ready, 10 / portTICK_PERIOD_MS);
            }
            
            // delay for 10s (since the inference may be heavy)
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void getInferenceResult(String& dLabel, float& dScore, int& dTimeMs) {
    dLabel = max_label;
    dScore = max_score;
    dTimeMs = infer_time;
}