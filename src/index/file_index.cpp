#include "file_index.h"

#include "io/simple_io.h"
#include "roo_logging.h"

namespace tapuino {

void FileIndexWriter::open(StringView path) {
  CHECK(!target_);
  target_ = fs_.open(String((const char *)path.data(), path.size()), "w");
  if (!target_) {
    status_ = BAD_FILE;
  }
}

void FileIndexWriter::close() {
  if (status_ != OK) return;
  target_.close();
  if (target_.getWriteError() != 0 || !write_path_.empty()) {
    status_ = WRITE_ERROR;
  }
}

void FileIndexWriter::containerBegin(ContainerType type, StringView name,
                                     uint32_t size) {
  LOG(INFO) << "Container begin for " << name << ", " << size;
  if (status_ != OK) return;
  write_path_.push_back(fpos_);
  addEntry(true, type, name, size);
}

void FileIndexWriter::addFile(FileType type, StringView name, uint32_t size) {
  if (status_ != OK) return;
  CHECK(!write_path_.empty());
  addEntry(false, type, name, size);
}

void FileIndexWriter::addEntry(bool container, uint8_t type, StringView name,
                               uint32_t size) {
  uint8_t buf[1024];
  writeU8(container ? 1 : 0, buf);
  uint8_t *cursor = buf + 3;  // leaving space for the record size
  uint32_t parent = (write_path_.empty() ? 0xFFFFFFFF : write_path_.back());
  cursor = writeU32(parent, cursor);  // parent, for fseek
  cursor = writeU8((uint8_t)type, cursor);
  cursor = writeU32(size, cursor);
  cursor = writeStr((const char *)name.data(), name.size(), cursor);
  uint16_t record_size = cursor - buf;
  writeU16(record_size, buf + 1);
  writeToFile(buf, record_size);
  fpos_ += record_size;
}

void FileIndexWriter::containerEnd() {
  LOG(INFO) << "Container end " << write_path_.size();
  if (status_ != OK) return;
  CHECK(!write_path_.empty());
  uint8_t container_end_marker = 0xFF;
  writeToFile(&container_end_marker, 1);
  write_path_.pop_back();
  fpos_ += 1;
}

void FileIndexWriter::writeToFile(const uint8_t *buf, size_t size) {
  while (size > 0 && status_ == OK) {
    size -= target_.write(buf, size);
    if (target_.getWriteError() != 0) {
      status_ = WRITE_ERROR;
    }
  }
}

// Reader

FileType FileIndexReader::Entry::file_type() const {
  CHECK(isFile());
  return (FileType)(type_ & 7);
};

ContainerType FileIndexReader::Entry::container_type() const {
  CHECK(isContainer());
  return (ContainerType)(type_ & 7);
};

uint32_t FileIndexReader::Entry::file_size() const {
  CHECK(isFile());
  return size_;
}

void FileIndexReader::open(StringView path) {
  CHECK(!input_);
  input_.set(fs_.open(String((const char *)path.data(), path.size()), "r"));
  if (!input_) {
    status_ = BAD_FILE;
  }
  path_.clear();
  dir_pushed_ = true;
  eof_ = false;
}

void FileIndexReader::close() {
  if (status_ != OK) return;
  input_.close();
}

bool FileIndexReader::readFromFile(uint8_t *buf, size_t len) {
  while (len > 0) {
    int r = input_.read(buf, len);
    if (r < 0) {
      return false;
    }
    buf += r;
    len -= r;
  }
  return true;
}

const FileIndexReader::Entry *FileIndexReader::next() {
  if (status_ != OK) {
    // Read error.
    return nullptr;
  }
  if (!input_) {
    // Not opened.
    return nullptr;
  }
  if (eof_) {
    // Finished reading.
    return nullptr;
  }
  if (!dir_pushed_) {
    path_.pop_back();
    dir_pushed_ = true;
  }
  while (true) {
    uint8_t type;
    if (!readFromFile(&type, 1)) {
      if (!path_.empty()) {
        status_ = PREMATURE_EOF;
      }
      return nullptr;
    }
    if (type == 0xFF) {
      // end-of-container marker. Skip over.
      if (path_.empty()) {
        // Unexpected dir end.
        status_ = BAD_DATA;
        return nullptr;
      }
      path_.pop_back();
      dir_pushed_ = true;
      if (path_.empty()) {
        // Legitimate EOF.
        eof_ = true;
        return nullptr;
      }
      continue;
    }
    uint8_t size_buf[2];
    if (!readFromFile(size_buf, 2)) {
      status_ = PREMATURE_EOF;
      return nullptr;
    }
    uint16_t record_size;
    readU16(record_size, size_buf);
    if (eof_) {
      status_ = PREMATURE_EOF;
      return nullptr;
    }
    uint8_t record_buf[record_size - 3];
    if (!readFromFile(record_buf, record_size - 3)) {
      status_ = PREMATURE_EOF;
      return nullptr;
    }
    const uint8_t *cursor = record_buf;
    uint32_t parent;
    uint8_t entry_type;
    uint32_t size;
    StringView name;
    cursor = readU32(parent, cursor);
    cursor = readU8(entry_type, cursor);
    entry_type &= 7;
    cursor = readU32(size, cursor);
    cursor = readStr(name, cursor);
    const Entry *parent_ptr = (path_.empty() ? nullptr : &path_.back());
    switch (type) {
      case 1: {
        // Container.
        path_.emplace_back(true, entry_type,
                           std::string((const char *)name.data(), name.size()),
                           size, parent_ptr);
        dir_pushed_ = true;
        break;
      }
      case 0: {
        // File.
        path_.emplace_back(false, entry_type,
                           std::string((const char *)name.data(), name.size()),
                           size, parent_ptr);
        dir_pushed_ = false;
        break;
      }
      default: {
        status_ = BAD_DATA;
        break;
      }
    }
    return path_.empty() ? nullptr : &path_.back();
  }
}

}  // namespace tapuino
