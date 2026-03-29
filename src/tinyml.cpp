#include "global.hpp"
#ifdef ENABLE_TINYML

#include "tinyml.h"
#include "temp_humid_mon.hpp"

int tinyMLReceiverCount = 0;

void tinyMLQueueReceiverCountInc()
{
    tinyMLReceiverCount++;
}

// Globals, for the convenience of one-shot setup.
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;

    // In order to use optimized tensorflow lite kernels, a signed int8_t quantized
    // model is preferred over the legacy unsigned model format. This means that
    // throughout this project, input images must be converted from unisgned to
    // signed format. The easiest and quickest way to convert from unsigned to
    // signed 8-bit integers is to subtract 128 from the unsigned value to get a
    // signed value.

    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 8 * 1024; // Adjust size based on your model
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

void setupTinyML()
{
    xAnomalyQueue = xQueueCreate(5, sizeof(float));
    Serial.println("TensorFlow Lite Init....");

    model = tflite::GetModel(dht_anomaly_model_tflite); // g_model_data is from model_data.h

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        MicroPrintf("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::MicroMutableOpResolver<5> micro_op_resolver;
    micro_op_resolver.AddAveragePool2D(tflite::Register_AVERAGE_POOL_2D_INT8());
    micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
    micro_op_resolver.AddDepthwiseConv2D(
        tflite::Register_DEPTHWISE_CONV_2D_INT8());
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddSoftmax(tflite::Register_SOFTMAX_INT8());
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        MicroPrintf("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

void tiny_ml_task(void *pvParameters)
{

    tempHumidMonQueueReceiverCountInc();
    static float temperature_l = 0.0;
    static float humidity_l = 0.0;

    setupTinyML();

    while (1)
    {

        // Prepare input data (e.g., sensor readings)
        // For a simple example, let's assume a single float input
        xQueueReceive(xTemperatureQueue, &temperature_l, 5 / portTICK_PERIOD_MS);
        xQueueReceive(xHumidityQueue, &humidity_l, 5 / portTICK_PERIOD_MS);
        input->data.f[0] = temperature_l;
        input->data.f[1] = humidity_l;

        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk)
        {
            MicroPrintf("Invoke failed");
            return;
        }

        // Get and process output
        float result = output->data.f[0];
        // for sigmoid output, abnormally is indicated by output close to 1, and normal is indicated by output close to 0.
        // here, for any value above 0.5, we consider it as anomaly, otherwise normal.
        Serial.println("Inference result: " + String(result) + " - " + String((result > 0.5) ? "Anomaly Detected" : "Normal"));
        for (int i = 0; i < tinyMLReceiverCount; i++)
        {
            xQueueSend(xAnomalyQueue, &result, 5 / portTICK_PERIOD_MS);
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

#endif // ENABLE_TINYML
