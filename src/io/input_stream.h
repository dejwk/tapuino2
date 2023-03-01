#pragma once

#include <inttypes.h>

#include <memory>

#include "FS.h"
#include "io/sd.h"

namespace tapuino {

class InputStreamImpl {
 public:
  virtual ~InputStreamImpl() { close(); }

  virtual int32_t read(uint8_t* buf, uint32_t count) { return -1; }
  virtual uint32_t size() const { return 0; }
  virtual bool ok() const { return true; }
  virtual void close() {}

 protected:
  InputStreamImpl() {}
};

class FileInputStreamImpl : public InputStreamImpl {
 public:
  FileInputStreamImpl(File file) : file_(std::move(file)) {}

  int32_t read(uint8_t* buf, uint32_t count) override {
    if (!file_) return -1;
    return file_.read(buf, count);
  }

  uint32_t size() const override { return file_.size(); }
  bool ok() const override { return file_; }
  void close() override { file_.close(); }

 private:
  File file_;
};

class InputStream {
 public:
  InputStream(std::unique_ptr<InputStreamImpl> impl)
      : impl_(std::move(impl)), num_bytes_read_(0) {}

  InputStream() : impl_(nullptr), num_bytes_read_(0) {}

  // Tries to read at least one byte, blocking if necessary. Returns zero on
  // EOF, negative value on error, and the number of bytes read otherwise.
  int32_t read(uint8_t* buf, uint32_t count) {
    int32_t read_now = impl_ == nullptr ? 0 : impl_->read(buf, count);
    if (read_now >= 0) num_bytes_read_ += read_now;
    return read_now;
  }

  // Tries to read the `count` bytes, blocking if necessary. Returns zero on
  // EOF, negative value on error, and the number of bytes read otherwise.
  int32_t readFully(uint8_t* buf, uint32_t count) {
    if (impl_ == nullptr) return 0;
    uint32_t read = 0;
    while (read < count) {
      int32_t read_now = impl_->read(buf + read, count - read);
      if (read_now < 0) {
        Serial.println("Returning error");
        return read_now;  // Error
      }
      if (read_now == 0) {
        // Reached EOF: return however many bytes read thus far.
        Serial.println("Reached EOF");
        break;
      }
      read += read_now;
    }
    num_bytes_read_ += read;
    Serial.printf("Returning %d; read now %d out of %d\n", read, numBytesRead(), size());
    return read;
  }

  void close() {
    if (impl_ != nullptr) impl_->close();
  }

  uint32_t size() { return impl_ != nullptr ? impl_->size() : 0; }

  uint32_t numBytesRead() const { return num_bytes_read_; }

  operator bool() const { return impl_ != nullptr ? impl_->ok() : true; }

 private:
  std::unique_ptr<InputStreamImpl> impl_;
  uint32_t num_bytes_read_;
};

}  // namespace tapuino