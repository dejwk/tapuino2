#pragma once

#include <functional>
#include <memory>
#include <stack>
#include <vector>

#include "catalog/catalog.h"
#include "io/sd.h"
#include "roo_logging.h"
#include "roo_scheduler.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/environment.h"

namespace tapuino {

class BrowsingActivity;

using TapFileSelectFn = std::function<void(const MemIndexEntry&)>;

class BrowsingActivity : public roo_windows::Activity {
 public:
  BrowsingActivity(const roo_windows::Environment& env,
                   roo_scheduler::Scheduler& scheduler, Sd& sd,
                   Catalog& catalog, TapFileSelectFn select_fn);

  void onStart() override;
  void onResume() override;
  void onPause() override;
  void onStop() override;

  roo_windows::Widget& getContents() override {
    return *CHECK_NOTNULL(contents_);
  }

  std::string currentPath() const;

  void setCwd(MemIndex::PathEntryId cd);
  void scrollToPos(uint16_t pos);
  void scrollToDim(roo_windows::YDim y);

  void onEntryClicked(int idx);
  void onParentDir();
  void onHomeClicked();

  void onFileDeleted(int idx);
  void onFileDeletedConfirmed(int idx);

 private:
  void checkCardPresent();

  roo_scheduler::Scheduler& scheduler_;
  roo_scheduler::RepetitiveTask card_checker_;
  std::unique_ptr<roo_windows::Widget> contents_;
  Sd& sd_;
  Catalog& catalog_;
  MemIndex::PathEntryId cd_;
  MemIndex::PathEntryId* cd_list_;
  int element_count_;  // Not including '..'
  TapFileSelectFn select_fn_;
};

}  // namespace tapuino