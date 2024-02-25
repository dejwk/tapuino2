#include "core/include/FlipBuffer.h"

#include "io/buffered_reader.h"
#include "io/input_stream.h"

using namespace TapuinoNext;

FlipBuffer::FlipBuffer(uint32_t bufferSize) {
  // ensure the buffer size is a power of 2
  uint64_t powerTwo = 2;
  while (powerTwo <= bufferSize) {
    powerTwo <<= 1;
  }
  powerTwo >>= 1;

  this->bufferSize = powerTwo;
  bufferMask = this->bufferSize - 1;
  halfBufferSize = this->bufferSize >> 1;
  bufferSwitchPos = halfBufferSize;

  pBuffer = NULL;
  bufferPos = 0;
}

FlipBuffer::~FlipBuffer() {
  if (pBuffer != NULL) {
    free(pBuffer);
    pBuffer = NULL;
  }
}

ErrorCodes FlipBuffer::Init() {
  if (pBuffer == NULL) {
    pBuffer = (uint8_t*)malloc(bufferSize);
    if (pBuffer == NULL) return ErrorCodes::OUT_OF_MEMORY;
  }

  bufferPos = 0;
  return ErrorCodes::OK;
}

void FlipBuffer::Reset() {
  if (pBuffer != NULL) {
    memset(pBuffer, 0, bufferSize);
    bufferPos = 0;
  }
}

ErrorCodes FlipBuffer::SetHeader(uint8_t* header, uint32_t size) {
  if (size > bufferSize) {
    return ErrorCodes::OUT_OF_RANGE;
  }

  memcpy(pBuffer, header, size);
  bufferPos = 0;
  return ErrorCodes::OK;
}

ErrorCodes FlipBuffer::FillWholeBuffer(tapuino::InputStream& tapFile,
                                       uint32_t atPos) {
  if (atPos < bufferSize) {
    if (tapFile.readFully(&pBuffer[atPos], bufferSize - atPos) < 0) {
      return ErrorCodes::FILE_ERROR;
    }
    bufferPos = 0;
    return ErrorCodes::OK;
  }
  return ErrorCodes::OUT_OF_RANGE;
}

uint8_t FlipBuffer::ReadByte() {
  if (bufferPos == bufferSwitchPos) {
    bufferSwitchFlag = true;
  }

  uint8_t ret = pBuffer[bufferPos++];
  bufferPos &= bufferMask;
  return ret;
}

ErrorCodes FlipBuffer::FillBufferIfNeeded(tapuino::InputStream& tapFile) {
  if (bufferSwitchFlag) {
    bufferSwitchFlag = false;
    bufferSwitchPos = halfBufferSize - bufferSwitchPos;
    int32_t read = tapFile.readFully(&pBuffer[bufferSwitchPos], halfBufferSize);
    if (read < 0) {
      return ErrorCodes::FILE_ERROR;
    }
  }
  return ErrorCodes::OK;
}

void FlipBuffer::WriteByte(uint8_t value) {
  if (bufferPos == bufferSwitchPos) {
    bufferSwitchFlag = true;
  }

  pBuffer[bufferPos++] = value;
  bufferPos &= bufferMask;
}

void FlipBuffer::FlushBufferIfNeeded(File tapFile) {
  if (bufferSwitchFlag) {
    bufferSwitchFlag = false;
    bufferSwitchPos = halfBufferSize - bufferSwitchPos;
    tapFile.write(&pBuffer[bufferSwitchPos], halfBufferSize);
  }
}

void FlipBuffer::FlushBufferFinal(File tapFile) {
  if (bufferPos < halfBufferSize) {
    tapFile.write(pBuffer, bufferPos);
  } else {
    tapFile.write(&pBuffer[halfBufferSize], bufferPos - halfBufferSize);
  }
}
