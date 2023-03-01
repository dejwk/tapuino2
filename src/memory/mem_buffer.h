#include <stdint.h>

#include "unzip.h"

namespace tapuino {
namespace membuf {

// The largest buffer that we can reliably memalloc when ESP32 starts.
constexpr uint32_t kIndexBufferSize = 107 * 1024;

constexpr uint32_t kIndexMaxEntries = 5000;

void Allocate();

// Note: mem index buffer and the unzip buffer are overlapping (they share the
// same underlying buffer of kIndexBufferSize bytes). They cannot be used at the
// same time.

uint8_t* GetMemIndexBuffer();
uint32_t* GetMemIndexEntriesBuffer();

uint16_t* GetMemIndexSortedByPathBuffer();
uint16_t* GetMemIndexSortedByNameBuffer();

uint16_t* GetWorkBuffer();

ZIPFILE& GetUnzipBuffer();

}  // namespace membuf
}  // namespace tapuino