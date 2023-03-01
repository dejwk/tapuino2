#include "tapuino.h"

#include "roo_windows/core/application.h"
#include "roo_windows/core/task.h"

using namespace roo_windows;

namespace tapuino {

Tapuino::Tapuino(const Environment& env, roo_scheduler::Scheduler& scheduler,
                 Sd& sd, MemIndex& mem_index)
    : options_(nullptr, nullptr),
      flip_buffer_(4096),
      utility_(nullptr, &options_, &flip_buffer_),
      start_(env, scheduler, sd, mem_index, indexer_, browser_),
      indexer_(env, scheduler, sd, mem_index),
      browser_(
          env, scheduler, sd, mem_index,
          [this](const tapuino::MemIndexEntry& e) { enterPlayer(e); }),
      player_(env, scheduler, sd, mem_index, &utility_) {
  flip_buffer_.Init();
}

void Tapuino::start(Application& application) {
  task_.reset(application.addTaskFullScreen());
  task_->enterActivity(&start_);
}

void Tapuino::enterPlayer(const tapuino::MemIndexEntry& e) {
  task_->enterActivity(&player_);
  player_.enter(e);
}

}  // namespace tapuino