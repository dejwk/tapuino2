#pragma once

#include <stdint.h>

#include <vector>

#include "mem_index.h"
#include "file_index.h"

namespace tapuino {

class MemIndexBuilder {
 public:
  MemIndexBuilder(MemIndex &mem_index);

  MemIndex &mem_index() { return mem_index_; }

  void reset();
  
  bool addEntry(const FileIndexReader::Entry *entry);
  void buildSortIndexes();

 private:
  MemIndex &mem_index_;
  std::vector<MemIndex::Handle> path_;
};

}  // namespace tapuino