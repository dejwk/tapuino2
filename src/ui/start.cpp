#include "start.h"

#include "roo_display/ui/string_printer.h"
#include "roo_windows/containers/aligned_layout.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/task.h"
#include "roo_windows/widgets/text_label.h"

#include "index/mem_index.h"
#include "memory/mem_buffer.h"

using namespace roo_windows;

namespace tapuino {

namespace {

class Contents : public AlignedLayout {
 public:
  Contents(const Environment &env)
      : AlignedLayout(env), label_(env, "", *env.theme().font.h6) {
    add(label_, roo_display::kCenter | roo_display::kMiddle);
  }

  void setText(std::string text) { label_.setText(text); }

 private:
  TextLabel label_;
};

}  // namespace

StartActivity::StartActivity(const Environment &env,
                             roo_scheduler::Scheduler &scheduler, Sd &sd,
                             MemIndex &mem_index, IndexingActivity &indexer,
                             BrowsingActivity &browser)
    : scheduler_(scheduler),
      card_checker_(
          scheduler, [this]() { checkCardPresent(); }, roo_time::Millis(250)),
      contents_(new Contents(env)),
      sd_(sd),
      mem_index_(mem_index),
      indexer_(indexer),
      browser_(browser),
      state_(NO_CARD) {}

void StartActivity::onResume() { 
  state_ = NO_CARD;
  Contents *contents = (Contents *)contents_.get();
  contents->setText("Looking for card...");
  card_checker_.start(roo_time::Millis(0));
}

void StartActivity::onPause() { card_checker_.stop(); }

bool allocated = false;

void StartActivity::checkCardPresent() {
  Contents *contents = (Contents *)contents_.get();
  if (!sd_.is_mounted() && !sd_.mount()) {
    getTask()->clearDialog();
    state_ = NO_CARD;
    contents->setText("Please insert a formatted SD card.");
    return;
  }
  switch (state_) {
    case NO_CARD: {
      state_ = MOUNTED;
      contents->setText("Reading index...");
      getApplication()->refresh();
      if (!allocated) {
        tapuino::membuf::Allocate();
        mem_index_.init();
        allocated = true;
      }
      LoadResult result = mem_index_.Load(sd_.fs(), kIndexFilePath);
      if (result.status == LoadResult::OK) {
        state_ = PRESENT;
        getTask()->enterActivity(&browser_);
      } else if (result.status == LoadResult::DOES_NOT_EXIST) {
        getTask()->showAlertDialog("Create index?",
                                   "This SD card needs to be indexed\n"
                                   "before use. It will take seconds\n"
                                   "to minutes, depending on the\n"
                                   "directory structure.",
                                   {"CANCEL", "CREATE INDEX"}, [this](int id) {
                                     buildIndexPromptResponse(id);
                                   });
      } else {
        getTask()->showAlertDialog(
            "Rebuild index?",
            roo_display::StringPrintf("Index read failure:\n%s",
                                      result.error_details.c_str()),
            {"CANCEL", "REBUILD INDEX"},
            [this](int id) { buildIndexPromptResponse(id); });
      }
      break;
    }
    default: {
    }
  }
}

void StartActivity::buildIndexPromptResponse(int id) {
  Contents *contents = (Contents *)contents_.get();
  if (id == 1) {
    // Confirmed.
    getTask()->enterActivity(&indexer_);
  } else {
    // Canceled.
    state_ = NO_INDEX;
    contents->setText("SD card is not indexed.");
  }
}

}  // namespace tapuino