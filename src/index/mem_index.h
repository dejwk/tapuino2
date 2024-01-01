#pragma once

#include <FS.h>
#include <stdint.h>

#include "roo_display/core/utf8.h"

namespace tapuino {

extern const char *kIndexFilePath;

using roo_display::StringView;

struct LoadResult {
  enum Status {
    OK = 0,
    DOES_NOT_EXIST = 1,
    PREMATURE_EOF = 2,
    UNSUPPORTED_VERSION = 3,
    IO_ERROR = 4,
  };

  Status status;
  std::string error_details;
};

class MemIndex {
 public:
  // Resolved ID pointing to a specific file.
  class Handle {
   public:
    constexpr Handle() : val_(0) {}
    constexpr Handle(const Handle &other) = default;

    Handle &operator++() {
      ++val_;
      return *this;
    }

    static constexpr Handle Root() { return Handle(0x0000); }
    static constexpr Handle None() { return Handle(0xFFFF); }

   private:
    friend class MemIndex;
    friend class MemIndexEntry;
    friend bool operator==(Handle a, Handle b);
    friend bool operator<(Handle a, Handle b);

    constexpr Handle(uint16_t val) : val_(val) {}
    uint16_t val_;
  };

  // an index into an array of all FS handles sorted by path.
  class PathEntryId {
   public:
    constexpr PathEntryId() : val_(0) {}
    constexpr PathEntryId(uint16_t val) : val_(val) {}
    constexpr PathEntryId(const PathEntryId &other) = default;

    PathEntryId operator++() {
      ++val_;
      return *this;
    }

    PathEntryId operator--() {
      --val_;
      return *this;
    }

   private:
    friend class MemIndex;
    friend class MemIndexEntry;
    friend bool operator==(PathEntryId a, PathEntryId b);
    friend bool operator<(PathEntryId a, PathEntryId b);

    uint16_t val_;
  };

  // an index into an array of 'file' handles sorted by name.
  class FileNameId {
   public:
    constexpr FileNameId() : val_(0) {}
    constexpr FileNameId(uint16_t val) : val_(val) {}
    constexpr FileNameId(const FileNameId &other) = default;

    FileNameId operator++() {
      ++val_;
      return *this;
    }

    FileNameId operator--() {
      --val_;
      return *this;
    }

   private:
    friend class MemIndex;
    friend class MemIndexEntry;
    friend bool operator==(FileNameId a, FileNameId b);
    friend bool operator<(FileNameId a, FileNameId b);

    uint16_t val_;
  };

  MemIndex();

  // Should be called from setup().
  void init();

  void clear();

  // Returns a fully-qualified file path for a given entry.
  std::string getPath(Handle h) const;

  uint32_t remainingCapacity() const;

  Handle end() const { return (Handle)count_; }

  size_t count() const { return count_; }
  size_t file_count() const { return file_count_; }

  Handle file_by_name(FileNameId i) const {
    return taps_sorted_by_name_[i.val_];
  }

  Handle entry_by_path(PathEntryId i) const {
    return all_sorted_by_path_[i.val_];
  }

  LoadResult Load(FS &fs, const char *filename);
  bool Store(FS &fs, const char *filename);

 private:
  friend class MemIndexEntry;
  friend class MemIndexBuilder;

  Handle addEntry(uint8_t type, Handle parent, StringView name,
                  uint32_t file_size);

  Handle addDir(Handle parent, StringView name) {
    return addEntry(0, parent, name, 0);
  }

  Handle addZip(Handle parent, StringView name, uint32_t file_size) {
    return addEntry(1, parent, name, file_size);
  }

  Handle addTapFile(Handle parent, StringView name, uint32_t file_size) {
    return addEntry(2, parent, name, file_size);
  }

  void buildSortIndexes();

  uint32_t *entries_;
  uint16_t count_;
  uint16_t capacity_;

  uint8_t *data_;
  uint32_t data_size_;

  Handle *all_sorted_by_path_;
  Handle *taps_sorted_by_name_;
  uint16_t file_count_;
};

inline bool operator==(MemIndex::Handle a, MemIndex::Handle b) {
  return a.val_ == b.val_;
}

inline bool operator!=(MemIndex::Handle a, MemIndex::Handle b) {
  return !(a == b);
}

inline bool operator<(MemIndex::Handle a, MemIndex::Handle b) {
  return a.val_ < b.val_;
}

inline bool operator==(MemIndex::PathEntryId a, MemIndex::PathEntryId b) {
  return a.val_ == b.val_;
}

inline bool operator!=(MemIndex::PathEntryId a, MemIndex::PathEntryId b) {
  return !(a == b);
}

inline bool operator<(MemIndex::PathEntryId a, MemIndex::PathEntryId b) {
  return a.val_ < b.val_;
}

inline bool operator>(MemIndex::PathEntryId a, MemIndex::PathEntryId b) {
  return b < a;
}

inline bool operator>=(MemIndex::PathEntryId a, MemIndex::PathEntryId b) {
  return !(a < b);
}

inline bool operator==(MemIndex::FileNameId a, MemIndex::FileNameId b) {
  return a.val_ == b.val_;
}

inline bool operator!=(MemIndex::FileNameId a, MemIndex::FileNameId b) {
  return !(a == b);
}

inline bool operator<(MemIndex::FileNameId a, MemIndex::FileNameId b) {
  return a.val_ < b.val_;
}

inline bool operator>(MemIndex::FileNameId a, MemIndex::FileNameId b) {
  return b < a;
}

inline bool operator>=(MemIndex::FileNameId a, MemIndex::FileNameId b) {
  return !(a < b);
}

class MemIndexEntry {
 public:
  MemIndexEntry();

  MemIndexEntry(const MemIndex *fs, MemIndex::Handle h);

  // uint16_t record_size() const;

  MemIndex::Handle parent_handle() const;

  MemIndexEntry parent() const;

  bool isContainer() const;
  bool isDir() const;
  bool isZip() const;
  bool isTapFile() const;

  bool isDescendantOf(MemIndex::Handle node) const;

  void printSize(char *out) const;

  // Returns a piece of the name that is identical to the parent directory's
  // name. Since it is a common pattern that ZIP files have similar names as
  // their entries, the trick of referencing that common suffix can save
  // considerable space. (+15% capacity at the same footprint for a sample
  // real-life TAP collection).
  uint8_t shared_name_prefix_len() const;

  // Returns the piece of the name that is unique.
  // An actual name is a concatenation of the shared prefix and the unique
  // suffix.
  roo_display::StringView unique_name_suffix() const;

  roo_display::StringView prefix() const;

  bool isRoot() const;

  // Returns a fully-qualified file path for this entry.
  std::string getPath() const;

  // Returns a simple name for this entry.
  std::string getName() const;

 protected:
  friend class MemIndex;

  uint32_t getEntry() const { return fs_->entries_[h_.val_]; }

  const uint8_t *getDataPtr() const {
    return fs_->data_ + (getEntry() & 0x1FFFF);
  }

  void appendName(std::string &result) const;

  void appendNamePrefix(uint8_t len, std::string &result) const;

  void appendPath(std::string &result) const;

  const MemIndex *fs_;
  MemIndex::Handle h_;
};

class MemIndexIterator {
 public:
  MemIndexIterator(MemIndex &fs)
      : fs_(fs), current_(MemIndex::Handle::Root()) {}

  bool next(MemIndexEntry &result) {
    if (current_ < fs_.end()) {
      result = MemIndexEntry(&fs_, current_);
      ++current_;
      return true;
    }
    return false;
  }

 private:
  MemIndex &fs_;
  MemIndex::Handle current_;
};

class MemIndexElementIterator {
 public:
  MemIndexElementIterator(MemIndex &fs, MemIndex::PathEntryId id)
      : fs_(fs), node_(fs_.entry_by_path(id)), current_(id) {}

  bool next(MemIndex::PathEntryId &result) {
    while (current_ < (int)fs_.count()) {
      ++current_;
      if (current_ >= (int)fs_.count()) return false;
      MemIndexEntry e(&fs_, fs_.entry_by_path(current_));
      if (e.parent_handle() == node_) {
        // Found!
        result = current_;
        return true;
      }
      if (!e.isDescendantOf(node_)) {
        current_ = fs_.count();
      }
    }
    return false;
  }

 private:
  MemIndex &fs_;
  MemIndex::Handle node_;
  MemIndex::PathEntryId current_;
};

}  // namespace tapuino