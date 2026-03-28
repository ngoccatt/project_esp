#include "image_concat.hpp"
#include "tinyml_img.h"
#include <mbedtls/base64.h>
#include <JPEGDEC.h>

// -------------------------------------------------------------------
// Model input configuration — must match your image model's input shape.
// The frontend already compresses to IMAGE_MAX_WIDTH x IMAGE_MAX_HEIGHT
// (currently 96x96 in script.js), so these should stay in sync.
// -------------------------------------------------------------------
static constexpr int MODEL_INPUT_W      = 96;
static constexpr int MODEL_INPUT_H      = 96;
static constexpr int MODEL_INPUT_CH     = 3;   // RGB channels
static constexpr int MODEL_INPUT_PIXELS = MODEL_INPUT_W * MODEL_INPUT_H * MODEL_INPUT_CH;

// Static intermediate buffers — avoids heap fragmentation on ESP32.
// JPEG_BUF_MAX: upper bound for a 96x96 JPEG at quality 0.65 is ~5KB;
// 24KB gives comfortable headroom for larger test images.
static constexpr size_t JPEG_BUF_MAX = 24 * 1024;
static uint8_t  s_jpegBuf[JPEG_BUF_MAX];                                    // raw JPEG bytes after base64 decode
static uint8_t  s_pixelBuf[MODEL_INPUT_W * MODEL_INPUT_H * 3];               // RGB888 pixels from JPEGDEC (3 bytes per pixel)
static int8_t   s_modelInput[MODEL_INPUT_PIXELS];                             // INT8-quantized pixels fed to model (pixel - 128)
static int      s_jpegDecodeW = 0;
static int      s_jpegDecodeH = 0;
static bool     s_newImageReceived = false; // Flag to indicate a new image has been received and processed

// -------------------------------------------------------------------
// STEP 2 helper — JPEGDEC per-block draw callback.
// JPEGDEC calls this for each decoded MCU block during jpeg.decode().
// pDraw->pPixels is uint8_t* laid out as R,G,B,R,G,B,... (RGB888).
// We copy each pixel's 3 bytes into s_pixelBuf at the correct (x,y).
// -------------------------------------------------------------------
static int jpegDrawCallback(JPEGDRAW *pDraw) {
    const uint8_t *src = (const uint8_t *)pDraw->pPixels;
    for (int row = 0; row < pDraw->iHeight; row++) {
        for (int col = 0; col < pDraw->iWidth; col++) {
            int dstX = pDraw->x + col;
            int dstY = pDraw->y + row;
            if (dstX < MODEL_INPUT_W && dstY < MODEL_INPUT_H) {
                int dstBase = (dstY * MODEL_INPUT_W + dstX) * 3;
                int srcBase = (row  * pDraw->iWidth  + col)  * 3;
                s_pixelBuf[dstBase + 0] = src[srcBase + 0]; // R
                s_pixelBuf[dstBase + 1] = src[srcBase + 1]; // G
                s_pixelBuf[dstBase + 2] = src[srcBase + 2]; // B
            }
        }
    }
    return 1; // 1 = continue decoding, 0 = abort
}

// -------------------------------------------------------------------
// STEP 1: Base64 string → raw JPEG bytes stored in s_jpegBuf.
// mbedtls_base64_decode is built into the ESP32 Arduino framework —
// no extra library installation needed.
// -------------------------------------------------------------------
static bool decodeBase64ToJpeg(const String &base64Str, size_t &outLen) {
    const unsigned char *src = (const unsigned char *)base64Str.c_str();
    size_t srcLen = base64Str.length();

    int ret = mbedtls_base64_decode(s_jpegBuf, JPEG_BUF_MAX, &outLen, src, srcLen);
    if (ret != 0) {
        Serial.printf("[Step 1] Base64 decode failed: %d\n", ret);
        return false;
    }
    Serial.printf("[Step 1] Base64 decoded: %u bytes JPEG\n", outLen);
    return true;
}

// -------------------------------------------------------------------
// STEP 2: Raw JPEG bytes → RGB888 pixel matrix in s_pixelBuf.
// Requires the JPEGDEC library (bitbank2/JPEGDEC — see platformio.ini).
// RGB888 output gives true 8-bit per channel with no precision loss,
// at the cost of 1.5× more pixel buffer RAM vs RGB565 (still fine on ESP32).
// -------------------------------------------------------------------
static bool decodeJpegToPixels(size_t jpegLen) {
    static JPEGDEC jpeg;
    memset(s_pixelBuf, 0, sizeof(s_pixelBuf));

    if (!jpeg.openRAM(s_jpegBuf, (int)jpegLen, jpegDrawCallback)) {
        Serial.println("[Step 2] JPEGDEC openRAM failed");
        return false;
    }

    s_jpegDecodeW = jpeg.getWidth();
    s_jpegDecodeH = jpeg.getHeight();
    Serial.printf("[Step 2] JPEG dimensions: %dx%d\n", s_jpegDecodeW, s_jpegDecodeH);

    jpeg.setPixelType(RGB8888);  // Request full 8-bit per channel output (no 565 roundtrip)
    if (!jpeg.decode(0, 0, 0)) {
        Serial.println("[Step 2] JPEGDEC decode failed");
        jpeg.close();
        return false;
    }
    jpeg.close();
    return true;
}

// -------------------------------------------------------------------
// STEP 3 + 4: Nearest-neighbor resize + INT8 quantization.
//   Input : s_pixelBuf — RGB888 at decoded JPEG resolution (3 bytes per pixel)
//   Output: s_modelInput — int8_t in layout [H][W][C]
//
// Quantization: int8 = (int)uint8_channel - 128
//   Equivalent to MobileNet float normalization (x/127.5 - 1.0) with
//   scale=1/128 and zero_point=0.  Avoids a separate float buffer and
//   the per-value quantization loop in tinyml_img.cpp.
//
// Because the frontend already resizes to MODEL_INPUT_W x MODEL_INPUT_H
// before sending, scaleX/scaleY will be 1.0 in normal operation and
// this step becomes a straight pixel copy + subtract-128.
// -------------------------------------------------------------------
static void resizeAndNormalize() {
    int srcW = (s_jpegDecodeW < MODEL_INPUT_W) ? s_jpegDecodeW : MODEL_INPUT_W;
    int srcH = (s_jpegDecodeH < MODEL_INPUT_H) ? s_jpegDecodeH : MODEL_INPUT_H;
    float scaleX = (float)srcW / MODEL_INPUT_W;
    float scaleY = (float)srcH / MODEL_INPUT_H;

    for (int y = 0; y < MODEL_INPUT_H; y++) {
        for (int x = 0; x < MODEL_INPUT_W; x++) {
            int srcX = (int)(x * scaleX);
            int srcY = (int)(y * scaleY);

            // Read R, G, B directly — no bit-unpacking needed with RGB888
            int srcBase = (srcY * MODEL_INPUT_W + srcX) * 3;
            uint8_t r = s_pixelBuf[srcBase + 0];
            uint8_t g = s_pixelBuf[srcBase + 1];
            uint8_t b = s_pixelBuf[srcBase + 2];

            // Quantize uint8 [0,255] → int8 [-128,127]: subtract 128
            int dstBase = (y * MODEL_INPUT_W + x) * MODEL_INPUT_CH;
            s_modelInput[dstBase + 0] = (int8_t)((int)r - 128);
            s_modelInput[dstBase + 1] = (int8_t)((int)g - 128);
            s_modelInput[dstBase + 2] = (int8_t)((int)b - 128);
        }
    }
    Serial.printf("[Step 3+4] Resized & INT8-quantized: %dx%dx%d ready\n",
                  MODEL_INPUT_W, MODEL_INPUT_H, MODEL_INPUT_CH);
}

namespace {
struct ImageTransferState {
    bool active = false;
    String imageId;
    int expectedChunks = 0;
    int receivedChunks = 0;
    String base64Payload;
};

ImageTransferState g_imageTransfer;

// -------------------------------------------------------------------
// Called once all chunks have been concatenated into a full Base64 string.
// Runs the complete 5-step image preparation + inference pipeline.
// -------------------------------------------------------------------
void onImageReassembled(const String &imageId, const String &base64Image) {
    Serial.printf("Image reassembled. imageId=%s, base64Len=%u\n",
                  imageId.c_str(), base64Image.length());

    // Step 1 — Base64 string → raw JPEG bytes
    size_t jpegLen = 0;
    if (!decodeBase64ToJpeg(base64Image, jpegLen)) return;

    // Step 2 — JPEG bytes → RGB565 pixel matrix
    if (!decodeJpegToPixels(jpegLen)) return;

    // Step 3 + 4 — Resize (nearest-neighbor) + normalize to float [-1, 1]
    resizeAndNormalize();

    // indicate that a new image has been received and processed.
    s_newImageReceived = true;

    // Step 5 — Feed float tensor into image model and run inference
    // the model is fully prepared, fetch the data = s_modelInput, size = MODEL_INPUT_PIXELS
    // call getModelInputBuffer(inputSize) to get the pointer and size for the model input tensor
}

void concatenateImageChunks(const String &chunkIndex,
                            const String &totalChunks,
                            const String &base64ImageData,
                            const String &imageId = "legacy") {
    int index = chunkIndex.toInt();
    int total = totalChunks.toInt();

    if (total <= 0 || index < 0) {
        Serial.printf("Invalid chunk metadata. index=%d total=%d\n", index, total);
        return;
    }

    if (!g_imageTransfer.active || g_imageTransfer.imageId != imageId) {
        imageConcatReset();
        g_imageTransfer.active = true;
        g_imageTransfer.imageId = imageId;
        g_imageTransfer.expectedChunks = total;
        g_imageTransfer.receivedChunks = 0;
        g_imageTransfer.base64Payload.reserve(total * base64ImageData.length());
    }

    // Keep a simple sequential contract to avoid high memory bookkeeping on ESP32.
    if (index != g_imageTransfer.receivedChunks) {
        Serial.printf("Chunk order mismatch for imageId=%s. expected=%d got=%d\n",
                      g_imageTransfer.imageId.c_str(),
                      g_imageTransfer.receivedChunks,
                      index);
        return;
    }

    g_imageTransfer.base64Payload += base64ImageData;
    g_imageTransfer.receivedChunks++;

    if (g_imageTransfer.receivedChunks == g_imageTransfer.expectedChunks) {
        onImageReassembled(g_imageTransfer.imageId, g_imageTransfer.base64Payload);
    }
}
} // namespace


bool getModelInputBuffer(uint8_t *&outBuffer, size_t &outSize) {
    if (s_newImageReceived) {
        outSize = MODEL_INPUT_PIXELS;
        outBuffer = (uint8_t *)s_modelInput; // point caller directly at static buffer — no copy
        s_newImageReceived = false; // reset the flag after providing the buffer
        return true;
    } else {
        return false; // No new image available
    }
}

bool imageConcatStart(const String &imageId, int totalChunks) {
    if (imageId.length() == 0 || totalChunks <= 0) {
        Serial.println("Invalid image_start message");
        return false;
    }

    imageConcatReset();
    g_imageTransfer.active = true;
    g_imageTransfer.imageId = imageId;
    g_imageTransfer.expectedChunks = totalChunks;
    g_imageTransfer.receivedChunks = 0;

    Serial.printf("Image transfer started. imageId=%s chunks=%d\n", imageId.c_str(), totalChunks);
    return true;
}

bool imageConcatAppendChunk(const String &imageId, int index, const String &base64Chunk) {
    if (!g_imageTransfer.active || imageId != g_imageTransfer.imageId) {
        Serial.println("Received image_chunk without valid image_start");
        return false;
    }

    concatenateImageChunks(String(index), String(g_imageTransfer.expectedChunks), base64Chunk, imageId);
    return true;
}

bool imageConcatFinish(const String &imageId) {
    if (!g_imageTransfer.active || imageId != g_imageTransfer.imageId) {
        Serial.println("Received image_end without active transfer");
        return false;
    }

    if (g_imageTransfer.receivedChunks != g_imageTransfer.expectedChunks) {
        Serial.printf("Image transfer incomplete. imageId=%s received=%d expected=%d\n",
                      g_imageTransfer.imageId.c_str(),
                      g_imageTransfer.receivedChunks,
                      g_imageTransfer.expectedChunks);
        imageConcatReset();
        return false;
    }

    // onImageReassembled is already called when the last chunk arrives.
    Serial.printf("Image transfer finished. imageId=%s\n", imageId.c_str());
    // this only reset the receiving data. processed image is still in s_modelInput for inference.
    imageConcatReset();
    return true;
}

void imageConcatReset() {
    g_imageTransfer.active = false;
    g_imageTransfer.imageId = "";
    g_imageTransfer.expectedChunks = 0;
    g_imageTransfer.receivedChunks = 0;
    g_imageTransfer.base64Payload = "";
}

