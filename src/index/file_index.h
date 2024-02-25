#pragma once

#include <FS.h>
#include <stdint.h>

#include <vector>

#include "io/buffered_reader.h"
#include "roo_display/core/utf8.h"

namespace tapuino {

using ::roo_display::StringView;

// typedef uint32_t FileIndexHandle;

enum FileType {
  TAP_FILE = 0,
};

enum ContainerType {
  DIR = 0,
  ZIP = 1,
};

enum Status {
  OK = 0,
  PREMATURE_EOF = 1,
  BAD_DATA = 2,
  READ_ERROR = 3,
  WRITE_ERROR = 4,
  BAD_FILE = 5,
};

class FileIndexWriter {
 public:
  FileIndexWriter(FS& fs) : fs_(fs), status_(OK) {}
  ~FileIndexWriter() { close(); }

  void open(StringView path);
  void close();

  void containerBegin(ContainerType type, StringView name, uint32_t size);
  void containerEnd();

  void addFile(FileType type, StringView name, uint32_t size);

  Status status() const { return status_; }

  operator bool() const { return status_ == OK; }

 private:
  void addEntry(bool container, uint8_t type, StringView name, uint32_t size);

  void writeToFile(const uint8_t* buf, size_t size);

  FS& fs_;
  File target_;
  uint32_t fpos_;
  std::vector<uint32_t> write_path_;
  Status status_;
};

class FileIndexReader {
 public:
  class Entry {
   public:
    Entry(bool is_container, uint8_t type, std::string name, uint32_t size,
          const Entry* parent)
        : is_container_(is_container),
          type_(type),
          name_(std::move(name)),
          size_(size),
          parent_(parent),
          depth_(parent == nullptr ? 0 : parent->depth_ + 1) {}

    bool isFile() const { return !is_container_; }
    bool isContainer() const { return is_container_; }

    bool isRoot() const { return parent_ == nullptr; }

    // How many ancestors. Zero means root.
    uint8_t depth() const { return depth_; }

    const Entry* parent() const { return parent_; }

    const std::string& name() const { return name_; }

    FileType file_type() const;
    uint32_t file_size() const;

    ContainerType container_type() const;

    std::string getPath() const;

    void appendPath(std::string& path) const;

   private:
    bool is_container_;
    uint8_t type_;
    std::string name_;
    uint32_t size_;
    const Entry* parent_;
    uint8_t depth_;
  };

  FileIndexReader(FS& fs) : fs_(fs), status_(OK) {
    // Make sure that pointers don't get invalidated.
    path_.reserve(20);
  }

  ~FileIndexReader() { close(); }

  operator bool() const { return input_ && status_ == OK; }

  void open(StringView path);
  void close();

  // Does not transfer ownership. The returned entry is valid until the
  // subsequent call to next(). Returns nullptr on EOS or error.
  const Entry* next();

 private:
  bool readFromFile(uint8_t* buf, size_t len);

  FS& fs_;
  // File input_;
  // Buffering speeds up index read by ~22%. Buffer sizes > 256 bytes don't make
  // much difference.
  BufferedReader input_;
  bool eof_;
  std::vector<Entry> path_;
  bool dir_pushed_;
  Status status_;
};

}  // namespace tapuino
