#pragma once

#include <functional>
#include <memory>
#include <stack>
#include <vector>

#include "index/mem_index.h"
#include "io/sd.h"
#include "roo_glog/logging.h"
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
                   MemIndex& mem_index, TapFileSelectFn select_fn);

  void onStart() override;
  void onResume() override;
  void onPause() override;
  void onStop() override;

  roo_windows::Widget& getContents() override {
    return *CHECK_NOTNULL(contents_);
  }

  void setCwd(MemIndex::PathEntryId cd, uint16_t first_pos);
  void onEntryClicked(int idx);
  void onParentDir();

 private:
  void checkCardPresent();

  roo_scheduler::Scheduler& scheduler_;
  roo_scheduler::RepetitiveTask card_checker_;
  std::unique_ptr<roo_windows::Widget> contents_;
  Sd& sd_;
  MemIndex& mem_index_;
  MemIndex::PathEntryId cd_;
  MemIndex::PathEntryId* cd_list_;
  int element_count_;  // Not including '..'
  TapFileSelectFn select_fn_;
};

}  // namespace tapuino