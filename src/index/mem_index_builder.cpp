#include "mem_index_builder.h"

#include "io/data_io.h"
#include "memory/mem_buffer.h"
#include "roo_logging.h"

namespace tapuino {

MemIndexBuilder::MemIndexBuilder(MemIndex &mem_index) : mem_index_(mem_index) {}

void MemIndexBuilder::reset() { mem_index_.clear(); }

bool MemIndexBuilder::addEntry(const FileIndexReader::Entry *entry) {
  while (entry->depth() < path_.size()) path_.pop_back();
  MemIndex::Handle parent =
      path_.empty() ? MemIndex::Handle::None() : path_.back();
  MemIndex::Handle added;
  if (entry->isContainer()) {
    if (entry->container_type() == DIR) {
      added = mem_index_.addDir(parent, entry->name());
    } else {
      added = mem_index_.addZip(parent, entry->name(), 0);
    }
  } else {
    added = mem_index_.addTapFile(parent, entry->name(), entry->file_size());
  }
  if (added == MemIndex::Handle::None()) return false;
  if (entry->isContainer()) {
    path_.push_back(added);
  }
  return true;
}

void MemIndexBuilder::buildSortIndexes() { mem_index_.buildSortIndexes(); }

}  // namespace tapuino