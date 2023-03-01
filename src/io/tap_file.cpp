#include "io/tap_file.h"

#include "io/input_stream.h"
#include "io/sd.h"
#include "io/unzipper.h"

namespace tapuino {

TapFile::TapFile(Sd& sd) : sd_(sd) {}

void TapFile::set(const MemIndexEntry& entry) {
  if (entry.parent().isZip()) {
    file_path_ = entry.parent().getPath();
    // Remove the trailing '/' after the zip file name.
    file_path_.pop_back();
    zip_entry_ = entry.getName();
  } else {
    file_path_ = entry.getPath();
  }
  simple_name_ = entry.getName();
}

// SdMount TapFile::mount() const { return SdMount(sd_); }

InputStream TapFile::open() {
  // SdMount mount(sd_);
  File file = sd_.fs().open(file_path_.c_str());
  if (!file) {
    return InputStream();
  }
  if (zip_entry_.empty()) {
    return InputStream(//std::move(mount),
                       std::unique_ptr<InputStreamImpl>(
                           new FileInputStreamImpl(std::move(file))));
  } else {
    unzipper::FileInfo fi;
    if (unzipper::OpenZip(sd_.fs(), file_path_.c_str()) != 0 ||
        unzipper::LocateFile(zip_entry_.c_str()) != 0 ||
        unzipper::OpenCurrentFile() != 0 ||
        unzipper::GetCurrentFileInfo(fi) != 0) {
      return InputStream();
    }
    return InputStream(
        // std::move(mount),
        std::unique_ptr<InputStreamImpl>(new unzipper::ZipEntryInputStreamImpl(
            std::move(file), fi.info.uncompressed_size)));
  }
}

}  // namespace tapuino