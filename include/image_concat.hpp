#ifndef _IMAGE_CONCAT_HPP_
#define _IMAGE_CONCAT_HPP_

#include <Arduino.h>

bool imageConcatStart(const String &imageId, int totalChunks);
bool imageConcatAppendChunk(const String &imageId, int index, const String &base64Chunk);
bool imageConcatFinish(const String &imageId);
void imageConcatReset();

bool getModelInputBuffer(uint8_t *&outBuffer, size_t &outSize);

#endif // _IMAGE_CONCAT_HPP_
