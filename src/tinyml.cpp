#include "tinyml.h"
#include "global.hpp"

// Globals, for the convenience of one-shot setup.
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 8 * 1024; // Adjust size based on your model
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model_tflite); // g_model_data is from model_data.h

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

void tiny_ml_task(void *pvParameters)
{

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
            error_reporter->Report("Invoke failed");
            return;
        }

        // Get and process output
        float result = output->data.f[0];
        // for sigmoid output, abnormally is indicated by output close to 1, and normal is indicated by output close to 0.
        // here, for any value above 0.5, we consider it as anomaly, otherwise normal. 
        Serial.println("Inference result: " + String(result) + " - " + String((result > 0.5) ? "Anomaly Detected" : "Normal"));

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}