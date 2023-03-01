#pragma once

#include "index/mem_index.h"
#include "io/input_stream.h"

namespace tapuino {

// Backed by either a regular file or a ZIP file.
class TapFile {
 public:
  TapFile(Sd& sd);

  void set(const MemIndexEntry& entry);

  // // Makes sure that the filesystem backing the file is mounted.
  // // The mounts use reference counting. The filesystem remains mounted as long
  // // as they are any SdMount objects alive. Each InputStream carries its own
  // // mount. The mounts can be copied and moved around.
  // SdMount mount() const;

  InputStream open();
  const std::string& name() const { return simple_name_; }

 private:
  Sd& sd_;
  std::string file_path_;
  // If non-ZIP, empty string.
  std::string zip_entry_;
  std::string simple_name_;
};

}  // namespace tapuino
