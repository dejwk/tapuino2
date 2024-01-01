#pragma once

// #include <functional>
// #include <memory>
// #include <stack>
// #include <vector>

// #include "FS.h"
// #include "SPI.h"

#include "index/mem_index.h"
#include "io/sd.h"
#include "roo_logging.h"
#include "roo_scheduler.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/dialogs/alert_dialog.h"
#include "ui/browser.h"
#include "ui/indexer.h"

namespace tapuino {

// Waits for the card to be inserted, and then loads the index and transitions
// to browsing. If the index isn't found, shows dialog, and if confirmed,
// transitions to indexing.
class StartActivity : public roo_windows::Activity {
 public:
  enum State {
    NO_CARD = 0,
    MOUNTED = 1,
    NO_INDEX = 2,
    PRESENT = 3,
  };

  StartActivity(const roo_windows::Environment& env,
                roo_scheduler::Scheduler& scheduler, Sd& sd,
                MemIndex& mem_index, IndexingActivity& indexer,
                BrowsingActivity& browser);

  void onPause() override;
  void onResume() override;

  roo_windows::Widget& getContents() override {
    return *CHECK_NOTNULL(contents_);
  }

 private:
  void checkCardPresent();
  void buildIndexPromptResponse(int id);

  roo_scheduler::Scheduler& scheduler_;
  roo_scheduler::RepetitiveTask card_checker_;
  std::unique_ptr<roo_windows::Widget> contents_;
  Sd& sd_;
  MemIndex& mem_index_;
  IndexingActivity& indexer_;
  BrowsingActivity& browser_;

  State state_;
};

}  // namespace tapuino