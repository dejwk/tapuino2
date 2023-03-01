#include "mem_buffer.h"

#include "esp_heap_caps.h"

namespace tapuino {
namespace membuf {

// We want to keep as much RAM as possible for the index, and we're running
// tight on RAM when we do so. Therefore, we allow the index to overlap with the
// zip memory buffer, since only one of them is used at the time. It means that
// we need to reload the index from disk every time we used the unzipper
// library.

// The maximum index size at which it does not overlap with the zip buffer, and
// does not need to be reloaded after a zip operation.
static constexpr int kMaxNonSharedIndexSize =
    (kIndexBufferSize - sizeof(ZIPFILE)) & 0xFFFFFFFC;

namespace {

union membuf_t {
  uint8_t idx_buffer[kIndexBufferSize];
  struct {
    // Used to keep the zip buffer at the higher range of memory, so that for
    // small indexes they don't overlap.
    uint8_t padding[kMaxNonSharedIndexSize];
    ZIPFILE zip;
  };
};

membuf_t& AllocateInternal() {
  static membuf_t* membuf = (membuf_t*)heap_caps_malloc(sizeof(membuf_t), MALLOC_CAP_DEFAULT);
  return *membuf;
}

}  // namespace

void Allocate() {
  AllocateInternal();
  GetMemIndexEntriesBuffer();
  GetMemIndexSortedByPathBuffer();
  GetMemIndexSortedByNameBuffer();
  GetWorkBuffer();
  //   heap_caps_print_heap_info(0);
}

uint8_t* GetMemIndexBuffer() { return AllocateInternal().idx_buffer; }

uint32_t* GetMemIndexEntriesBuffer() {
  static uint32_t* buf = new uint32_t[kIndexMaxEntries];
  return buf;
}

uint16_t* GetMemIndexSortedByPathBuffer() {
  static uint16_t* buf = new uint16_t[kIndexMaxEntries];
  return buf;
}

uint16_t* GetMemIndexSortedByNameBuffer() {
  static uint16_t* buf = new uint16_t[kIndexMaxEntries];
  return buf;
}

uint16_t* GetWorkBuffer() {
  static uint16_t* buf = new uint16_t[kIndexMaxEntries];
  return buf;
}

ZIPFILE& GetUnzipBuffer() { return AllocateInternal().zip; }

}  // namespace membuf
}  // namespace tapuino
