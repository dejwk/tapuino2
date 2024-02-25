#include <inttypes.h>

#include <FS.h>

#include "roo_display/core/utf8.h"

namespace tapuino {

template <typename Stream>
bool readAll(Stream& s, uint8_t *dest, int count) {
  while (count > 0 && s) {
    int result = s.read(dest, count);
    if (result < 0) return false;
    count -= result;
    dest += result;
  }
  return true;
}

class BufferedReader {
 public:
  BufferedReader() : rem_(-1) {}

  void set(File f) {
    file_ = f;
    rem_ = f.read(buf_, 256);
    pos_ = 0;
    eof_ = false;
  }

  int read(uint8_t* dest, int count) {
    if (count <= rem_) {
      memcpy(dest, &buf_[pos_], count);
      rem_ -= count;
      pos_ += count;
      return count;
    }
    if (rem_ < 0) return rem_;
    memcpy(dest, &buf_[pos_], rem_);
    int result = rem_;
    if (file_.available() == 0) {
      eof_ = true;
      rem_ = -1;
      pos_ = 0;
      return -1;
    }
    rem_ = file_.read(buf_, 256);
    pos_ = 0;
    return result;
  }

  uint8_t readU8() {
    uint8_t val;
    read(&val, 1);
    return val;
  }

  uint16_t readU16() {
    uint8_t val[2];
    if (!readAll(*this, val, 2)) return -1;
    return val[0] << 8 | val[1];
  }

  uint32_t readU32() {
    uint8_t val[4];
    if (!readAll(*this, val, 4)) return -1;
    return val[0] << 24 | val[1] << 16 | val[2] << 8 | val[3];
  }

  operator bool() const { return rem_ >= 0; }

  bool eof() const { return eof_; }

  void close() {
    file_.close();
    rem_ = -1;
  }

 private:
  File file_;
  uint8_t buf_[256];
  int rem_;
  int pos_;
  bool eof_;
};

}  // namespace tapuino