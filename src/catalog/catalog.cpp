#include "catalog/catalog.h"

#include <limits>
#include <string>

#include "index/file_index.h"
#include "index/mem_index_builder.h"

namespace tapuino {

const char *kIndexDir = "/__tapuino";
const char *kMasterIndex = "/__tapuino/master.idx";
const char *kMemIndex = "/__tapuino/mem.idx";
const char *kMasterIndexTmp = "/__tapuino/master.idx.new";
const char *kMemIndexTmp = "/__tapuino/mem.idx.new";
const char *kTransactionFile = "/__tapuino/transaction";

enum TransactionType {
  DELETE = 0,
  RENAME = 1,
  CREATE = 2,
};

Catalog::PositionInParent Catalog::getPositionInParent(
    MemIndex::PathEntryId id) const {
  MemIndexEntry entry = resolve(id);
  if (entry.isRoot()) {
    return PositionInParent{.parent = MemIndex::PathEntryId(0), .position = 0};
  }
  MemIndex::Handle parent_handle = entry.parent_handle();
  MemIndex::PathEntryId current = id;
  uint16_t pos = 0;
  // Iterate back over the file index, until reaching the parent dir. Count
  // entries that are our (older) siblings.
  while (current > MemIndex::PathEntryId(0)) {
    --current;
    MemIndex::Handle h = resolvePathEntryId(current);
    if (h == parent_handle) {
      return PositionInParent{.parent = current, .position = pos};
    }
    MemIndexEntry e(&mem_index_, h);
    if (e.parent_handle() == parent_handle) {
      // Sibling found.
      ++pos;
    }
  }
}

MemIndexEntry Catalog::resolve(MemIndex::PathEntryId id) const {
  return MemIndexEntry(&mem_index_, resolvePathEntryId(id));
}

MemIndex::Handle Catalog::resolvePathEntryId(MemIndex::PathEntryId id) const {
  return mem_index_.entry_by_path(id);
}

bool Catalog::deletePath(MemIndex::PathEntryId id) {
  MemIndex::Handle handle = resolvePathEntryId(id);
  MemIndexEntry entry(&mem_index_, handle);

  FS &fs = sd_.fs();
  File t = fs.open(kTransactionFile, "w");
  if (!t) {
    return false;
  }
  t.write(DELETE);
  std::string path = entry.getPath();
  t.write(path.size());
  t.write((const uint8_t *)path.c_str(), path.size());
  if (!t) {
    fs.remove(kTransactionFile);
    return false;
  }
  t.close();

  return deletePath(handle);
}

namespace {

bool deleteDirRecursively(FS &fs, File file) {
  while (File f = file.openNextFile()) {
    if (f.isDirectory()) {
      if (strcmp(f.name(), ".") == 0 || strcmp(f.name(), "..") == 0) {
        continue;
      }
      deleteDirRecursively(fs, f);
      return fs.rmdir(file.path());
    } else {
      return fs.remove(f.path());
    }
  }
}

bool deleteRecursively(FS &fs, const char *path) {
  if (!fs.exists(path)) return true;
  File f = fs.open(path);
  if (!f) return false;
  if (f.isDirectory()) {
    return deleteDirRecursively(fs, std::move(f));
  } else {
    return fs.remove(path);
  }
}

}  // namespace

bool Catalog::deletePath(MemIndex::Handle handle) {
  MemIndexEntry entry(&mem_index_, handle);
  std::string path_to_delete = entry.getPath();
  FS &fs = sd_.fs();

  // 1. Create new file index (if the old one exists).
  FileIndexReader file_index_reader(fs);
  if (!fs.exists(kMasterIndex)) {
    // Perhaps we already created the new index and deleted the old one?
    if (!fs.exists(kMasterIndexTmp)) return false;
    // We assume that index creation was successful and the old index has been
    // deleted.
  } else {
    fs.remove(kMasterIndexTmp);
    FileIndexWriter file_index_writer(fs);
    file_index_writer.open(kMasterIndexTmp);
    if (!file_index_writer) return false;

    file_index_reader.open(kMasterIndex);
    if (!file_index_reader) return false;

    int depth = 0;
    int container_deletion_depth = std::numeric_limits<int>::max();
    std::string path;
    while (true) {
      const FileIndexReader::Entry *entry = file_index_reader.next();
      if (entry == nullptr) break;
      while (entry->depth() < depth) {
        file_index_writer.containerEnd();
        --depth;
      }
      if (entry->depth() > container_deletion_depth) {
        // We're in the subdirectory that is getting deleted - ignore.
        continue;
      } else if (entry->depth() == container_deletion_depth) {
        // Make sure that once we're out of the subdirectory getting deleted,
        // we're done deleting.
        container_deletion_depth = std::numeric_limits<int>::max();
      }
      depth = entry->depth();
      std::string path = entry->getPath();
      if (path == path_to_delete) {
        if (entry->isFile()) {
          // Simply skip.
        } else {
          container_deletion_depth = entry->depth();
        }
        continue;
      }
      if (entry->isContainer()) {
        file_index_writer.containerBegin(
            entry->container_type(), entry->name(),
            entry->container_type() == ZIP ? entry->file_size() : 0);
      } else {
        file_index_writer.addFile(entry->file_type(), entry->name(),
                                  entry->file_size());
      }
    }
    while (depth > 0) {
      file_index_writer.containerEnd();
      --depth;
    }
    if (!file_index_reader) return false;
    file_index_reader.close();
    file_index_writer.close();
    if (file_index_writer.status() != OK) return false;
  }

  // 2. Create new memory index.
  fs.remove(kMemIndexTmp);

  file_index_reader.open(kMasterIndexTmp);
  if (!file_index_reader) return false;
  MemIndexBuilder mem_index_builder(mem_index_);
  mem_index_.clear();
  while (true) {
    const FileIndexReader::Entry *entry = file_index_reader.next();
    if (entry == nullptr) break;
    mem_index_builder.addEntry(entry);
  }
  mem_index_builder.buildSortIndexes();

  if (!mem_index_.Store(fs, kMemIndexTmp)) return false;

  // 3. Actually delete.

  String target(path_to_delete.c_str(), path_to_delete.size());
  if (!deleteRecursively(fs, path_to_delete.c_str())) {
    return false;
  }

  // 4. Commit the file index.

  fs.remove(kMasterIndex);
  if (!fs.rename(kMasterIndexTmp, kMasterIndex)) return false;

  // 5. Commit the mem index.

  fs.remove(kMemIndex);
  if (!fs.rename(kMemIndexTmp, kMemIndex)) return false;

  // 6. Delete the transaction file.

  if (!fs.remove(kTransactionFile)) return false;

  return true;
}

}  // namespace tapuino
