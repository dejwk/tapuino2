#pragma once

#include "FS.h"
#include "io/input_stream.h"
#include "unzipLIB.h"

namespace tapuino {
namespace unzipper {

int OpenZip(FS &fs, const char *filename);
int CloseZip();
int OpenCurrentFile();
int CloseCurrentFile();
int ReadCurrentFile(uint8_t *buffer, size_t len);
int GotoFirstFile();
int GotoNextFile();
int LocateFile(const char *filename);

struct FileInfo {
  unz_file_info info;
  char filename[256];
  char extra_field[128];
  char comment[256];
};

int GetCurrentFileInfo(FileInfo &info);

class ZipEntryInputStreamImpl : public InputStreamImpl {
 public:
  ZipEntryInputStreamImpl(File file, int entry_size)
      : file_(std::move(file)), entry_size_(entry_size) {}

  int32_t read(uint8_t *buf, uint32_t count) override {
    return ReadCurrentFile(buf, count);
  }

  uint32_t size() const override { return entry_size_; }

  bool ok() const override { return file_; }

  void close() override {
    CloseCurrentFile();
    CloseZip();
    file_.close();
  }

 private:
  File file_;
  int entry_size_;
};

}  // namespace unzipper
}  // namespace tapuino
