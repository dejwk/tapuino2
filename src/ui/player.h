#pragma once

#include <memory>
#include <stack>
#include <vector>

#include "FS.h"
#include "SPI.h"
#include "core/include/ESP32TapLoader.h"
#include "index/mem_index.h"
#include "io/tap_file.h"
#include "roo_glog/logging.h"
#include "roo_scheduler.h"
#include "roo_windows/core/activity.h"
#include "roo_windows/core/environment.h"

namespace tapuino {

class PlayerActivity;

class PlayerActivity : public roo_windows::Activity {
 public:
  PlayerActivity(const roo_windows::Environment& env,
                 roo_scheduler::Scheduler& scheduler, Sd& sd,
                 MemIndex& mem_index, TapuinoNext::UtilityCollection* utility);

  void onStart() override;
  void onResume() override;
  void onPause() override;
  void onStop() override;

  roo_windows::Widget& getContents() override {
    return *CHECK_NOTNULL(contents_);
  }

  void enter(const MemIndexEntry& entry);

  void play();
  void stop();
  void rewind();

 private:
  void updateLoaderStatus();
  void loadFinished(TapuinoNext::ErrorCodes result);

  roo_scheduler::Scheduler& scheduler_;
  Sd& sd_;
  MemIndex& mem_index_;
  std::unique_ptr<roo_windows::Widget> contents_;
  TapFile tap_file_;

  // Whether the UI displays the view as playing.
  bool shows_playing_;

  TapuinoNext::ESP32TapLoader loader_;
  roo_scheduler::RepetitiveTask player_status_updater_;
};

}  // namespace tapuino