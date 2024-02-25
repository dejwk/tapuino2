#pragma once

#include "index/mem_index.h"
#include "io/sd.h"

namespace tapuino {

class Catalog {
 public:
  struct PositionInParent {
    MemIndex::PathEntryId parent;
    int position;
  };

  Catalog(Sd& sd, MemIndex& mem_index) : sd_(sd), mem_index_(mem_index) {}

  Sd& sd() { return sd_; }
  MemIndex& mem_index() { return mem_index_; }

  // Determines the parent container of the given entry, and the index under
  // which the given entry appears within it. Returns 0 if the id points at
  // root.
  PositionInParent getPositionInParent(MemIndex::PathEntryId id) const;

  // Deletes the file or directory pointed to by the specified path index entry.
  bool deletePath(MemIndex::PathEntryId path_entry_id);

  // Return the underlying mem index entry, corresponding to the given path ID.
  MemIndexEntry resolve(MemIndex::PathEntryId path_entry_id) const;

 private:
  // Returns a handle that corresponds to the specified entry from the path
  // sort index.
  MemIndex::Handle resolvePathEntryId(
      MemIndex::PathEntryId path_entry_id) const;

  // Deletes the file or directory pointed to by the specified handle.
  bool deletePath(MemIndex::Handle handle);

  Sd& sd_;
  MemIndex& mem_index_;
};

}  // namespace tapuino
