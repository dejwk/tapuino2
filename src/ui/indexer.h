#pragma once

#include <memory>
#include <stack>
#include <vector>

#include "index/file_index.h"
#include "index/mem_index.h"
#include "index/mem_index_builder.h"
#include "io/sd.h"
#include "roo_logging.h"
#include "roo_scheduler.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/environment.h"

namespace tapuino {

class IndexingActivity;

extern const char *kIndexDir;
extern const char *kMasterIndex;
extern const char *kMemIndex;
extern const char *kMasterIndexTmp;
extern const char *kMemIndexTmp;

class IndexBuilder : public roo_scheduler::IteratingTask::Iterator {
 public:
  enum Status {
    IN_PROGRESS = 0,
    SUCCESS = 1,
    ABORTED = 2,
    READ_ONLY_FS = 3,
    IO_ERROR = 4,
  };

  IndexBuilder(IndexingActivity& activity, roo_scheduler::Scheduler& scheduler,
               Sd& sd, MemIndex& mem_index);

  int64_t next() override;

  Status status() const { return status_; }
  const std::string& error_details() const { return error_details_; }
  bool file_idx_ok() const { return file_idx_ok_; }

  void abort();

  ~IndexBuilder() { LOG(INFO) << "File index builder deleted"; }

 private:
  class IndexWriterPathElem {
   public:
    IndexWriterPathElem(std::string dirname)
        : dirname_(std::move(dirname)), committed_(false) {}

    const std::string& dirname() const { return dirname_; }
    bool is_committed() const { return committed_; }
    void mark_committed() { committed_ = true; }

   private:
    std::string dirname_;
    bool committed_;
  };

  // Successive stages of indexing.
  void stageMount();
  void stageMkIdxDir();
  void stageTryMasterIndex();
  void stageInitMasterIndexBuild();
  void stageContinueMasterIndexBuild();
  void stageBuildMemoryIndex();
  void stageAddSortIndexes();
  void stageWriteMemoryIndex();

  void commitPath();
  void scanZipFile(File file);

  bool buildMemIndex(FileIndexReader& reader);

  IndexingActivity& activity_;
  Sd& sd_;
  void (IndexBuilder::*stage_)();
  Status status_;
  std::string error_details_;

  int tap_files_found_;

  roo_scheduler::IteratingTask itr_task_;
  std::vector<File> scan_dir_path_;
  std::vector<IndexWriterPathElem> index_writer_path_;
  FileIndexWriter index_writer_;
  MemIndex& mem_index_;
  MemIndexBuilder mem_index_builder_;
  bool file_idx_ok_;
};

class IndexingActivity : public roo_windows::Activity {
 public:
  IndexingActivity(const roo_windows::Environment& env,
                   roo_scheduler::Scheduler& scheduler, Sd& sd,
                   MemIndex& mem_index);

  void onStart() override;
  void onStop() override;

  roo_windows::Widget& getContents() override {
    return *CHECK_NOTNULL(contents_);
  }

  void setStage(roo_display::StringView stage);
  void setScannedDir(roo_display::StringView dir);
  void addTapFile(roo_display::StringView file, int total_count);

 private:
  friend class IndexBuilder;

  void startScan();
  void scanFinished();

  void buildMemIdx();

  roo_scheduler::Scheduler& scheduler_;
  Sd& sd_;
  std::unique_ptr<roo_windows::Widget> contents_;
  std::unique_ptr<IndexBuilder> index_builder_;
  MemIndex& mem_index_;
};

}  // namespace tapuino