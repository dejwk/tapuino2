#include <inttypes.h>

#include <FS.h>

#include "roo_display/core/utf8.h"

namespace tapuino {

template <typename Stream>
void writeAll(Stream& s, const uint8_t *buf, size_t size) {
  while (size > 0 && s) {
    size_t written = s.write(buf, size);
    size -= written;
    buf += written;
  }
}

class BufferedWriter {
 public:
  BufferedWriter() : fill_(0) {}

  void set(File f) {
    file_ = f;
    fill_ = 0;
  }

  void write(const uint8_t* src, size_t count) {
    if (fill_ > 0) {
      if (count < (256 - fill_)) {
        memcpy(&buf_[fill_], src, count);
        fill_ += count;
        return;
      }
      memcpy(&buf_[fill_], src, 256 - fill_);
      src += (256 - fill_);
      count -= (256 - fill_);
      writeAll(file_, buf_, 256);
      fill_ = 0;
    }
    while (count >= 256) {
      writeAll(file_, src, 256);
      src += 256;
      count -= 256;
    }
    memcpy(buf_, src, count);
    fill_ = count;
    return;
  }

  operator bool() const { return file_; }

  void close() {
    writeAll(file_, buf_, fill_);
    file_.close();
    fill_ = 0;
  }

  void writeU8(uint8_t v) {
    write(&v, 1);
  }

  void writeU16(uint16_t v) {
    writeU8(v >> 8);
    writeU8(v & 0xFF);
  }

  void writeU32(uint32_t v) {
    writeU8(v >> 24);
    writeU8((v >> 16) & 0xFF);
    writeU8((v >> 8) & 0xFF);
    writeU8((v >> 0) & 0xFF);
  }

 private:
  File file_;
  uint8_t buf_[256];
  size_t fill_;
};

}  // namespace tapuino