#include "mem_index.h"

#include <errno.h>

#include <cstring>

#include "io/buffered_reader.h"
#include "io/buffered_writer.h"
#include "io/data_io.h"
#include "memory/mem_buffer.h"
#include "roo_logging.h"
#include "strcmp.h"

namespace tapuino {

const char *kIndexFilePath = "/__tapuino/mem.idx";

namespace {

size_t find_common_prefix(const char *a, const char *b) {
  size_t n = 0;
  while (*a && *a++ == *b++) ++n;
  return n;
}

// 13-bit encoding: 3-bit decimal exponent + 10-bit mantissa.
// The 'exponent' is tuned for human formatting of KiB, MiB, etc: the lowest 3*N
// exponents are 2^(10/3), and the higher ones are 10.
uint16_t encodeBiSize(int32_t size) {
  int exponent = 0;
  while (size >= 1024 * 1024) {
    exponent += 3;
    size /= 1024;
  }
  if (size >= 100 * 1024) {
    size /= 1024;
    exponent += 3;
  } else if (size >= 10 * 1024) {
    size *= 10;
    size /= 1024;
    exponent += 2;
  } else if (size >= 1024) {
    size *= 100;
    size /= 1024;
    exponent += 1;
  }
  if (exponent > 7) {
    exponent = 7;
    size = 1023;
  }
  return (exponent << 10) + size;
}

constexpr int kParentEntryOffset = 0;
constexpr int kNamePrefixOffset = kParentEntryOffset + 2;
constexpr int kUniqueNameSuffixOffset = kNamePrefixOffset + 1;

void printEntrySize(uint32_t entry, char *out) {
  uint16_t encoded = (entry >> 17) & 0x1FFF;
  int exponent = (encoded >> 10) & 7;
  int size = encoded & 1023;
  int suffix_idx = 0;
  const char *suffix[] = {"B", "KiB", "MiB", "GiB"};
  while (exponent >= 3) {
    ++suffix_idx;
    exponent -= 3;
  }
  if (exponent == 0) {
    sprintf(out, "%d %s", size, suffix[suffix_idx]);
    return;
  }
  ++suffix_idx;
  if (exponent == 1) {
    int rem = size % 100;
    if (rem == 0) {
      sprintf(out, "%d %s", size / 100, suffix[suffix_idx]);
    } else {
      sprintf(out, "%d.%02d %s", size / 100, rem, suffix[suffix_idx]);
    }
  } else if (exponent == 2) {
    int rem = size % 10;
    if (rem == 0) {
      sprintf(out, "%d %s", size / 10, suffix[suffix_idx]);
    } else {
      sprintf(out, "%d.%d %s", size / 10, rem, suffix[suffix_idx]);
    }
  }
}

// bool isEntryContainer(uint32_t entry) { return (entry & 0x80000000) == 0; }

// bool isEntryFile(uint32_t entry) { return !isEntryContainer(entry); }

// bool isEntryDir(uint32_t entry) { return (entry & 0xC0000000) == 0; }

// bool isEntryZip(uint32_t entry) {
//   return isEntryContainer(entry) && !isEntryDir(entry);
// }

// uint32_t getEntryDataOffset(uint32_t entry) { return entry & 0x0001FFFF; }

}  // namespace

MemIndex::MemIndex()
    : entries_(nullptr),
      count_(0),
      capacity_(membuf::kIndexMaxEntries),
      data_(nullptr),
      data_size_(0),
      all_sorted_by_path_(nullptr),
      taps_sorted_by_name_(nullptr),
      file_count_(0) {}

void MemIndex::clear() {
  count_ = 0;
  data_size_ = 0;
  file_count_ = 0;
}

void MemIndex::init() {
  entries_ = membuf::GetMemIndexEntriesBuffer();
  data_ = membuf::GetMemIndexBuffer();
  all_sorted_by_path_ =
      (MemIndex::Handle *)membuf::GetMemIndexSortedByPathBuffer();
  taps_sorted_by_name_ =
      (MemIndex::Handle *)membuf::GetMemIndexSortedByNameBuffer();
}

uint32_t MemIndex::remainingCapacity() const {
  return membuf::kIndexBufferSize - data_size_;
}

std::string MemIndex::getPath(Handle h) const {
  MemIndexEntry entry(this, h);
  return entry.getPath();
}

MemIndex::Handle MemIndex::addEntry(uint8_t type, Handle parent,
                                    StringView name, uint32_t file_size) {
  if (count_ == capacity_) {
    LOG(ERROR) << "Overflow: the number of entries reached the limit of "
               << capacity_;
    return MemIndex::Handle::None();
  }
  // May be within a ZIP file, in which case the name is qualified.
  int name_len = name.size();
  int prefix_len = 0;
  const char *prefix = "";
  const char *delim = strrchr((const char *)name.data(), '/');
  if (delim != nullptr) {
    int new_name_len = delim - (const char *)name.data();
    prefix_len = name_len - new_name_len - 1;
  }
  if (name_len > 255) name_len = 255;
  if (prefix_len > 255) prefix_len = 255;
  std::string parent_name;
  if (parent != MemIndex::Handle::None()) {
    MemIndexEntry p(this, parent);
    p.appendName(parent_name);
  }
  int shared_prefix_len =
      find_common_prefix(parent_name.c_str(), (const char *)name.data());

  uint16_t record_size = kUniqueNameSuffixOffset + 1 + name_len -
                         shared_prefix_len + 1 + prefix_len;
  if (record_size > remainingCapacity()) {
    LOG(ERROR) << "Overflow; remaining capacity = " << remainingCapacity();
    return MemIndex::Handle::None();
  }
  uint16_t idx = count_++;
  entries_[idx] = (((type & 3) << 30) | (encodeBiSize(file_size) << 17) |
                   (data_size_ & 0x1FFFF));

  uint8_t *cursor = data_ + data_size_;
  cursor = writeU16(parent.val_, cursor);
  cursor = writeU8(shared_prefix_len, cursor);
  cursor = writeStr((const char *)name.data() + shared_prefix_len,
                    (uint8_t)(name_len - shared_prefix_len), cursor);
  cursor = writeStr(prefix, (uint8_t)prefix_len, cursor);
  assert(data_ + data_size_ + record_size == cursor);
  data_size_ += record_size;
  return (Handle)idx;
}

MemIndexEntry::MemIndexEntry(const MemIndex *fs, MemIndex::Handle h)
    : fs_(fs), h_(h) {}

MemIndexEntry::MemIndexEntry() : fs_(nullptr), h_(MemIndex::Handle::None()) {}

MemIndex::Handle MemIndexEntry::parent_handle() const {
  uint16_t v;
  readU16(v, getDataPtr());
  return (MemIndex::Handle)v;
}

MemIndexEntry MemIndexEntry::parent() const {
  CHECK(!isRoot());
  return MemIndexEntry(fs_, parent_handle());
}

bool MemIndexEntry::isContainer() const {
  return (getEntry() & 0x80000000) == 0;
}

bool MemIndexEntry::isContainerReadOnly() const {
  MemIndexEntry tmp = *this;
  while (true) {
    if (tmp.isZip()) {
      return true;
    }
    if (tmp.isRoot()) {
      return false;
    }
    tmp = tmp.parent();
  }
}


bool MemIndexEntry::isDir() const { return (getEntry() & 0xC0000000) == 0; }

bool MemIndexEntry::isZip() const {
  return (getEntry() & 0xC0000000) == 0x40000000;
}

bool MemIndexEntry::isTapFile() const {
  return (getEntry() & 0xC0000000) == 0x80000000;
}

void MemIndexEntry::printSize(char *out) const {
  printEntrySize(getEntry(), out);
}

uint8_t MemIndexEntry::shared_name_prefix_len() const {
  return *(getDataPtr() + kNamePrefixOffset);
}

roo_display::StringView MemIndexEntry::unique_name_suffix() const {
  roo_display::StringView v;
  readStr(v, getDataPtr() + kUniqueNameSuffixOffset);
  return v;
}

roo_display::StringView MemIndexEntry::prefix() const {
  roo_display::StringView v;
  readStr(v, getDataPtr() + kUniqueNameSuffixOffset + 1 +
                 unique_name_suffix().size());
  return v;
}

bool MemIndexEntry::isRoot() const {
  return (parent_handle() == MemIndex::Handle::None());
}

bool MemIndexEntry::isDescendantOf(MemIndex::Handle node) const {
  if (node == parent_handle()) return true;
  return isRoot() ? false : parent().isDescendantOf(node);
}

std::string MemIndexEntry::getPath() const {
  std::string path;
  appendPath(path);
  return path;
}

void MemIndexEntry::appendPath(std::string &path) const {
  if (!isRoot()) {
    parent().appendPath(path);
    path += "/";
  }
  roo_display::StringView p = prefix();
  if (!p.empty()) {
    path.append((const char *)p.data(), p.size());
  }
  appendName(path);
}

size_t MemIndexEntry::appendPath(char *result, size_t max_len) const {
  size_t len = 0;
  if (!isRoot()) {
    len += parent().appendPath(result, max_len);
    if (len >= max_len) return len;
  }
  roo_display::StringView p = prefix();
  if (!p.empty()) {
    size_t plen = max_len - len;
    if (plen > p.size()) plen = p.size();
    memcpy(&result[len], (const char *)p.data(), plen);
    len += plen;
    if (len >= max_len) return len;
    result[len] = '/';
    ++len;
    if (len >= max_len) return len;
  }
  len += appendName(&result[len], max_len - len);
  if (len >= max_len) return len;
  if (isContainer()) {
    result[len] = '/';
    ++len;
  }
  return len;
}

std::string MemIndexEntry::getName() const {
  std::string name;
  appendName(name);
  return name;
}

void MemIndexEntry::appendName(std::string &path) const {
  uint8_t shared_prefix = shared_name_prefix_len();
  if (shared_prefix > 0) {
    parent().appendNamePrefix(shared_prefix, path);
  }
  roo_display::StringView suffix = unique_name_suffix();
  path.append((const char *)suffix.data(), suffix.size());
}

size_t MemIndexEntry::appendName(char *result, size_t max_len) const {
  size_t len = 0;
  if (max_len == 0) return 0;
  uint8_t shared_prefix = shared_name_prefix_len();
  if (shared_prefix > 0) {
    if (shared_prefix > max_len) {
      shared_prefix = max_len;
    }
    parent().appendNamePrefix(shared_prefix, result);
    if (shared_prefix == max_len) return max_len;
    len = shared_prefix;
  }
  roo_display::StringView suffix = unique_name_suffix();
  size_t slen = max_len - len;
  if (slen > suffix.size()) slen = suffix.size();
  memcpy(&result[len], (const char *)suffix.data(), slen);
  len += slen;
  return len;
}

void MemIndexEntry::appendNamePrefix(uint8_t len, std::string &path) const {
  uint8_t shared_prefix = shared_name_prefix_len();
  if (shared_prefix > 0) {
    if (shared_prefix > len) shared_prefix = len;
    parent().appendNamePrefix(shared_prefix, path);
    len -= shared_prefix;
  }
  roo_display::StringView suffix = unique_name_suffix();
  path.append((const char *)suffix.data(), len);
}

void MemIndexEntry::appendNamePrefix(uint8_t len, char *result) const {
  uint8_t shared_prefix = shared_name_prefix_len();
  if (shared_prefix > 0) {
    if (shared_prefix > len) shared_prefix = len;
    parent().appendNamePrefix(shared_prefix, result);
    len -= shared_prefix;
    result += shared_prefix;
  }
  roo_display::StringView suffix = unique_name_suffix();
  memcpy(result, (const char *)suffix.data(), len);
}

namespace {

void RecursivelyExpandHandlePath(const MemIndexEntry &e, MemIndex::Handle **h) {
  if (e.isRoot()) {
    // Don't append; roots are always equal.
    return;
  }
  RecursivelyExpandHandlePath(e.parent(), h);
  *(*h)++ = e.handle();
}

// Writes out the path of file handles for the entry e, into the array pointed
// by `path`. Omits the root. Returns the length of the path, less one. That is,
// returns the number of elements set in the `path` array.
size_t ExpandHandlePath(const MemIndexEntry &e, MemIndex::Handle *path) {
  MemIndex::Handle *start = path;
  RecursivelyExpandHandlePath(e, &path);
  return path - start;
}

// Compares two paths lexicographically, ignoring case.
//
// First, determines and rejects the common prefix, using file handles (which
// are much faster to compare than names, since they are just integers). Then,
// compares the names of the first path elements that are actually different.
// (Note that when handles differ (after rejecting the common prefix), the names
// must differ, too, since names are unique per directory).
int MemIndexEntryCmp(const MemIndexEntry &a, const MemIndexEntry &b) {
  MemIndex::Handle ah[10];
  MemIndex::Handle bh[10];
  size_t ahl = ExpandHandlePath(a, ah);
  size_t bhl = ExpandHandlePath(b, bh);
  size_t common = ahl < bhl ? ahl : bhl;
  for (int i = 0; i < common; ++i) {
    const MemIndex::Handle &ahi = ah[i];
    const MemIndex::Handle &bhi = bh[i];
    if (ahi != bhi) {
      // Found different dir; must be different name.
      char an[64];
      char bn[64];
      MemIndexEntry ad(a.fs(), ahi);
      MemIndexEntry bd(b.fs(), bhi);
      an[ad.appendName(an, 63)] = 0;
      bn[bd.appendName(bn, 63)] = 0;
      StrToLwrExt((unsigned char*)an);
      StrToLwrExt((unsigned char*)bn);
      return strcmp(an, bn) < 0;
      // return StrCiCmp(an, bn) < 0;
    }
  }
  // Common prefix; the shorter dir is less-than.
  return ahl < bhl;
}

}  // namespace

void MemIndex::buildSortIndexes() {
  LOG(INFO) << "Building sort index by path...";
  for (uint16_t i = 0; i < count_; ++i) {
    all_sorted_by_path_[i] = i;
  }
  std::sort(all_sorted_by_path_, all_sorted_by_path_ + count_,
            [&](Handle &i, Handle &j) {
              MemIndexEntry a(this, i);
              MemIndexEntry b(this, j);
              return MemIndexEntryCmp(a, b);
              // char ap[256];
              // char bp[256];
              // ap[a.appendPath(ap, 255)] = 0;
              // bp[b.appendPath(bp, 255)] = 0;
              // // StrToLwrExt((unsigned char*)ap);
              // // StrToLwrExt((unsigned char*)bp);
              // // char* p1 = ap;
              // // char* p2 = bp;
              // // for (; ; p1++, p2++) {
              // //   int iDiff = *p1 - *p2;
              // //   if (iDiff != 0 || !*p1 || !*p2) {
              // //     return iDiff < 0;
              // //   }
              // // }
              // // return false;

              // // return strcmp(ap, bp) < 0;

              // // return false;
              // // printf("%s %s\n", ap, bp);
              // return strcasecmp(ap, bp) < 0;
              // // return MemIndexEntryCmp(a, b) < 0;
              // // return StrCiCmp(a.getPath().c_str(), b.getPath().c_str()) <
              // 0;
              // // return StrCiCmp(ap, bp) < 0;
            });
  LOG(INFO) << "Building sort index by path done.";
  LOG(INFO) << "Building sort index for files by name...";
  file_count_ = 0;
  for (uint16_t i = 0; i < count_; ++i) {
    MemIndexEntry a(this, (Handle)i);
    if (a.isTapFile()) {
      taps_sorted_by_name_[file_count_++] = i;
    }
  }
  std::sort(taps_sorted_by_name_, taps_sorted_by_name_ + file_count_,
            [&](Handle &i, Handle &j) {
              MemIndexEntry a(this, i);
              MemIndexEntry b(this, j);
              return a.getName() < b.getName();
            });
  LOG(INFO) << "Building sort index for files by name done.";
  // heap_caps_print_heap_info(0);
}

bool MemIndex::Store(FS &fs, const char *filename) {
  File f = fs.open(filename, "w");
  if (!f) return false;
  BufferedWriter writer;
  writer.set(f);
  writer.writeU16(0x0101);
  writer.writeU16(count_);
  for (uint16_t i = 0; i < count_; ++i) {
    writer.writeU32(entries_[i]);
  }
  writer.writeU32(data_size_);
  writer.write(data_, data_size_);
  for (uint16_t i = 0; i < count_; ++i) {
    writer.writeU16(all_sorted_by_path_[i].val_);
  }
  writer.writeU16(file_count_);
  for (uint16_t i = 0; i < file_count_; ++i) {
    writer.writeU16(taps_sorted_by_name_[i].val_);
  }
  writer.close();
  return f.getWriteError() == 0;
}

LoadResult MemIndex::Load(FS &fs, const char *filename) {
  Serial.println("Loading index into memory");
  File f = fs.open(filename, "r");
  if (!f) {
    if (errno == ENOENT) {
      return LoadResult{.status = LoadResult::DOES_NOT_EXIST};
    } else {
      return LoadResult{.status = LoadResult::IO_ERROR,
                        .error_details = strerror(errno)};
    }
  }
  BufferedReader reader;
  reader.set(f);
  uint16_t version = reader.readU16();
  if (reader.eof()) {
    return LoadResult{.status = LoadResult::PREMATURE_EOF,
                      .error_details = "premature end of file."};
  }
  if (version != 0x0101) {
    return LoadResult{
        .status = LoadResult::UNSUPPORTED_VERSION,
        .error_details = "unrecognized version or file corrupted."};
  }
  count_ = reader.readU16();
  if (reader.eof()) {
    return LoadResult{.status = LoadResult::PREMATURE_EOF,
                      .error_details = "premature end of file."};
  }
  for (uint16_t i = 0; i < count_; ++i) {
    entries_[i] = reader.readU32();
  }
  data_size_ = reader.readU32();
  if (reader.eof()) {
    return LoadResult{.status = LoadResult::PREMATURE_EOF,
                      .error_details = "premature end of file."};
  }
  readAll(reader, data_, data_size_);
  for (uint16_t i = 0; i < count_; ++i) {
    all_sorted_by_path_[i] = reader.readU16();
  }
  file_count_ = reader.readU16();
  if (reader.eof()) {
    return LoadResult{.status = LoadResult::PREMATURE_EOF,
                      .error_details = "premature end of file."};
  }
  for (uint16_t i = 0; i < file_count_; ++i) {
    taps_sorted_by_name_[i] = reader.readU16();
  }
  if (!f) {
    Serial.println("Loading index into memory FAILED");
    if (reader.eof()) {
      return LoadResult{.status = LoadResult::PREMATURE_EOF,
                        .error_details = "premature end of file."};
    } else {
      return LoadResult{.status = LoadResult::IO_ERROR,
                        .error_details = strerror(errno)};
    }
  }
  f.close();
  return LoadResult{.status = LoadResult::OK};
}

}  // namespace tapuino