#pragma once

#include "catalog/catalog.h"
#include "index/mem_index.h"
#include "io/sd.h"
#include "roo_logging.h"
#include "roo_scheduler.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/environment.h"
#include "roo_windows/dialogs/alert_dialog.h"
#include "ui/browser.h"
#include "ui/indexer.h"
#include "ui/player.h"
#include "ui/start.h"

#include "core/include/UtilityCollection.h"

namespace tapuino {

class Tapuino {
 public:
  Tapuino(const roo_windows::Environment& env,
          roo_scheduler::Scheduler& scheduler, Sd& sd, MemIndex& mem_index);

 public:
  void start(roo_windows::Application& application);

 private:
  void enterPlayer(const tapuino::MemIndexEntry& e);

  TapuinoNext::Options options_;

  TapuinoNext::FlipBuffer flip_buffer_;
  TapuinoNext::UtilityCollection utility_;

  Catalog catalog_;

  StartActivity start_;
  IndexingActivity indexer_;
  BrowsingActivity browser_;
  PlayerActivity player_;

  std::unique_ptr<roo_windows::Task> task_;
};

}  // namespace tapuino