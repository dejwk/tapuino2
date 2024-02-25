#include "indexer.h"

#include "index/mem_index_builder.h"
#include "io/unzipper.h"
#include "memory/mem_buffer.h"
#include "roo_display/core/utf8.h"
#include "roo_display/ui/string_printer.h"
#include "roo_smooth_fonts/NotoSans_Condensed/12.h"
#include "roo_smooth_fonts/NotoSans_Condensed/15.h"
#include "roo_smooth_fonts/NotoSans_Condensed/18.h"
// #include "roo_toolkit/log/log.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/panel.h"
#include "roo_windows/widgets/text_label.h"

using namespace roo_windows;

namespace tapuino {

IndexingActivity::IndexingActivity(const Environment &env,
                                   roo_scheduler::Scheduler &scheduler, Sd &sd,
                                   MemIndex &mem_index)
    : scheduler_(scheduler),
      sd_(sd),
      contents_(nullptr),
      mem_index_(mem_index) {}

class IndexerPanel : public VerticalLayout {
 public:
  IndexerPanel(const Environment &env, std::function<void()> abort_fn)
      : VerticalLayout(env),
        stage_(env, "", roo_display::font_NotoSans_Condensed_15(),
               roo_display::kLeft | roo_display::kMiddle),
        scan_details_(env),
        files_found_(env, "", roo_display::font_NotoSans_Condensed_18(),
                     roo_display::kLeft | roo_display::kMiddle),
        dir_scanned_(env, "", roo_display::font_NotoSans_Condensed_15(),
                     roo_display::kLeft | roo_display::kMiddle),
        recent_tap_added_(env, "", roo_display::font_NotoSans_Condensed_12(),
                          roo_display::kLeft | roo_display::kMiddle),
        abort_(env, "ABORT"),
        abort_fn_(abort_fn) {
    stage_.setPadding(PaddingSize::PADDING_SMALL);
    stage_.setMargins(MarginSize::MARGIN_NONE);
    add(stage_);

    scan_details_.setPadding(PADDING_NONE);
    scan_details_.setMargins(MARGIN_NONE);
    add(scan_details_);

    files_found_.setPadding(PaddingSize::PADDING_SMALL);
    files_found_.setMargins(MarginSize::MARGIN_NONE);

    dir_scanned_.setPadding(PaddingSize::PADDING_SMALL);
    dir_scanned_.setMargins(MarginSize::MARGIN_NONE);

    recent_tap_added_.setPadding(PaddingSize::PADDING_SMALL);
    recent_tap_added_.setMargins(MarginSize::MARGIN_NONE);

    scan_details_.add(files_found_);
    scan_details_.add(dir_scanned_);
    scan_details_.add(recent_tap_added_);
    scan_details_.setVisibility(Widget::GONE);

    abort_.setPadding(PADDING_HUGE, PADDING_REGULAR);
    abort_.setMargins(MARGIN_HUGE);
    add(abort_, VerticalLayout::Params().setGravity(kHorizontalGravityCenter));
    abort_.setOnInteractiveChange([this]() {
      getTask()->showAlertDialog("Abort scan?", "", {"CANCEL", "ABORT"},
                                 [this](int id) {
                                   if (id == 1) abort_fn_();
                                 });
    });

    updateFilesFound(0);
  }

  void setStage(roo_display::StringView stage) { stage_.setText(stage); }

  void setScannedDir(roo_display::StringView dir) {
    scan_details_.setVisibility(Widget::VISIBLE);
    if (!dir.empty()) {
      dir_scanned_.setTextf("Scanning: %s", dir.data());
    } else {
      dir_scanned_.setText("Scanning finished.");
    }
  }

  void addTapFile(roo_display::StringView file, int total_count) {
    updateFilesFound(total_count);
    recent_tap_added_.setTextf("Added: %s", (const char *)file.data());
  }

 private:
  void updateFilesFound(int found) {
    files_found_.setTextf("TAP files found: %d", found);
  }
  TextLabel stage_;
  VerticalLayout scan_details_;
  TextLabel files_found_;
  TextLabel dir_scanned_;
  TextLabel recent_tap_added_;
  SimpleButton abort_;
  std::function<void()> abort_fn_;
};

void IndexingActivity::onStart() {
  contents_.reset(new IndexerPanel(getApplication()->env(), [this]() {
    if (index_builder_ != nullptr) {
      index_builder_->abort();
    }
  }));
  startScan();
}

void IndexingActivity::startScan() {
  index_builder_.reset(new IndexBuilder(*this, scheduler_, sd_, mem_index_));
}

void IndexingActivity::scanFinished() {
  getTask()->clearDialog();
  if (index_builder_->status() == IndexBuilder::SUCCESS) {
    index_builder_.reset(nullptr);
    exit();
  } else {
    if (!index_builder_->file_idx_ok()) {
      // Best-effort attempt to delete likely corrupted master index.
      sd_.fs().remove(kMasterIndex);
    }
    sd_.fs().remove(kMemIndex);
    getTask()->showAlertDialog("Indexing failed",
                               index_builder_->error_details(), {"OK"},
                               [this](int) { exit(); });
    index_builder_.reset(nullptr);
  }
}

void IndexingActivity::onStop() { contents_.reset(nullptr); }

IndexBuilder::IndexBuilder(IndexingActivity &activity,
                           roo_scheduler::Scheduler &scheduler, Sd &sd,
                           MemIndex &mem_index)
    : activity_(activity),
      sd_(sd),
      stage_(&IndexBuilder::stageMount),
      status_(IN_PROGRESS),
      tap_files_found_(0),
      itr_task_(scheduler, *this, [this]() { activity_.scanFinished(); }),
      index_writer_(sd_.fs()),
      mem_index_(mem_index),
      mem_index_builder_(mem_index),
      file_idx_ok_(false) {
  itr_task_.start();
}

namespace {

int ends_with(const char *str, const char *suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);

  return (str_len >= suffix_len) &&
         (!memcmp(str + str_len - suffix_len, suffix, suffix_len));
}

}  // namespace

void IndexingActivity::setStage(roo_display::StringView stage) {
  ((IndexerPanel *)contents_.get())->setStage(stage);
}

void IndexingActivity::setScannedDir(roo_display::StringView dir) {
  ((IndexerPanel *)contents_.get())->setScannedDir(dir);
}

void IndexingActivity::addTapFile(roo_display::StringView file,
                                  int total_count) {
  ((IndexerPanel *)contents_.get())->addTapFile(file, total_count);
}

void IndexBuilder::abort() {
  if (status_ == IN_PROGRESS) {
    status_ = ABORTED;
    error_details_ = "Aborted by user.";
  }
}

int64_t IndexBuilder::next() {
  (this->*stage_)();
  return status_ == IN_PROGRESS ? 0 : -1;
}

void IndexBuilder::stageMount() {
  if (!sd_.mount()) {
    status_ = ABORTED;
    activity_.exit();
    return;
  }
  stage_ = &IndexBuilder::stageMkIdxDir;
  activity_.setStage("Creating index directory...");
}

void IndexBuilder::stageMkIdxDir() {
  if (sd_.fs().exists(kIndexDir)) {
    File idx = sd_.fs().open(kIndexDir);
    if (!idx) {
      status_ = IO_ERROR;
      error_details_ = strerror(errno);
      return;
    }
    if (!idx.isDirectory()) {
      if (!sd_.fs().remove(kIndexDir)) {
        status_ = IO_ERROR;
        error_details_ = strerror(errno);
        return;
      }
    }
  }
  if (!sd_.fs().mkdir(kIndexDir)) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  stage_ = &IndexBuilder::stageTryMasterIndex;
  activity_.setStage("Trying to read an existing master index...");
}

void IndexBuilder::stageTryMasterIndex() {
  File master_idx = sd_.fs().open(kMasterIndex);
  if (master_idx) {
    if (!master_idx.isDirectory()) {
      mem_index_builder_.reset();
      FileIndexReader reader(sd_.fs());
      if (buildMemIndex(reader)) {
        file_idx_ok_ = true;
        stage_ = &IndexBuilder::stageAddSortIndexes;
        activity_.setStage("Creating sort indexes...");
        return;
      }
    }
    sd_.fs().remove(kMasterIndex);
  }
  stage_ = &IndexBuilder::stageInitMasterIndexBuild;
  activity_.setStage("Initializing the master index...");
}

void IndexBuilder::stageInitMasterIndexBuild() {
  File root = sd_.fs().open("/");
  if (!root) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  scan_dir_path_.push_back(root);
  index_writer_path_.push_back(IndexWriterPathElem(""));
  index_writer_.open(kMasterIndex);
  if (!index_writer_) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  // Make sure that the root dir is written even if there are no TAP files in
  // it.
  commitPath();
  stage_ = &IndexBuilder::stageContinueMasterIndexBuild;
  activity_.setStage("Scanning directory tree...");
}

void IndexBuilder::stageContinueMasterIndexBuild() {
  File dir = scan_dir_path_.back();
  do {
    File f = dir.openNextFile();
    if (f.getWriteError()) {
      status_ = IO_ERROR;
      error_details_ = strerror(errno);
      return;
    }
    if (!f) {
      // End of the directory.
      scan_dir_path_.pop_back();
      if (index_writer_path_.back().is_committed()) {
        index_writer_.containerEnd();
      }
      index_writer_path_.pop_back();
      if (scan_dir_path_.empty()) {
        // We're done!
        index_writer_.close();
        stage_ = &IndexBuilder::stageBuildMemoryIndex;
        activity_.setStage("Building the memory index...");
      }
      return;
    }
    if (strcmp(f.name(), ".") == 0) continue;
    if (strcmp(f.name(), "..") == 0) continue;
    activity_.setScannedDir(f.path());
    if (f.isDirectory()) {
      index_writer_path_.push_back(IndexWriterPathElem(f.name()));
      scan_dir_path_.push_back(f);
      // Keep the file open so that it can be recursively listed.
    } else if (ends_with(f.name(), ".tap") || ends_with(f.name(), ".TAP") ||
               ends_with(f.name(), ".Tap")) {
      commitPath();
      index_writer_.addFile(TAP_FILE, f.name(), f.size());
      activity_.addTapFile(f.path(), ++tap_files_found_);
      activity_.getContents().getApplication()->refresh();
      f.close();
    } else if (ends_with(f.name(), ".zip") || ends_with(f.name(), ".ZIP") ||
               ends_with(f.name(), ".Zip")) {
      scanZipFile(f);
      f.close();
    }
  } while (false);
}

void IndexBuilder::stageBuildMemoryIndex() {
  File master_idx = sd_.fs().open(kMasterIndex);
  if (!master_idx) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  mem_index_builder_.reset();
  FileIndexReader reader(sd_.fs());
  if (!buildMemIndex(reader)) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  stage_ = &IndexBuilder::stageAddSortIndexes;
  activity_.setStage("Creating sort indexes...");
}

bool IndexBuilder::buildMemIndex(FileIndexReader &reader) {
  reader.open(kMasterIndex);
  if (!reader) return false;
  while (true) {
    const FileIndexReader::Entry *entry = reader.next();
    if (!reader) return false;
    if (entry == nullptr) return true;
    mem_index_builder_.addEntry(entry);
  }
}

void IndexBuilder::stageAddSortIndexes() {
  mem_index_builder_.buildSortIndexes();
  stage_ = &IndexBuilder::stageWriteMemoryIndex;
  activity_.setStage("Writing the memory index...");
}

void IndexBuilder::stageWriteMemoryIndex() {
  if (!mem_index_.Store(sd_.fs(), kMemIndex)) {
    status_ = IO_ERROR;
    error_details_ = strerror(errno);
    return;
  }
  status_ = SUCCESS;
}

void IndexBuilder::commitPath() {
  for (IndexWriterPathElem &elem : index_writer_path_) {
    if (!elem.is_committed()) {
      index_writer_.containerBegin(DIR, elem.dirname(), 0);
      elem.mark_committed();
    }
  }
}

void IndexBuilder::scanZipFile(File file) {
  int rc = unzipper::OpenZip(sd_.fs(), file.path());
  bool committed = false;
  if (rc == UNZ_OK) {
    unzipper::GotoFirstFile();
    rc = UNZ_OK;
    unzipper::FileInfo fi;
    do {
      rc = unzipper::GetCurrentFileInfo(fi);
      if (rc == UNZ_OK) {
        Serial.print(fi.filename);
        Serial.print(" - ");
        Serial.print(fi.info.compressed_size, DEC);
        Serial.print("/");
        Serial.println(fi.info.uncompressed_size, DEC);
      }
      if (ends_with(fi.filename, ".tap") || ends_with(fi.filename, ".TAP") ||
          ends_with(fi.filename, ".Tap")) {
        if (!committed) {
          commitPath();
          index_writer_.containerBegin(ZIP, file.name(), file.size());
          committed = true;
        }
        index_writer_.addFile(TAP_FILE, fi.filename, fi.info.uncompressed_size);
        activity_.addTapFile(
            roo_display::StringPrintf("%s/%s", file.path(), fi.filename),
            ++tap_files_found_);
      }
      rc = unzipper::GotoNextFile();
    } while (rc == UNZ_OK);
    unzipper::CloseZip();
  }
  if (committed) {
    index_writer_.containerEnd();
  }
}

void appendParentPath(const FileIndexReader::Entry *entry, std::string &s) {
  if (entry->parent() != nullptr) {
    appendParentPath(entry->parent(), s);
    s += "/";
  }
  s += entry->name();
}

}  // namespace tapuino