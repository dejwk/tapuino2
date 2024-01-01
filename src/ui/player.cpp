#include "player.h"

#include "SD.h"
#include "memory/mem_buffer.h"
#include "roo_display/core/utf8.h"
#include "roo_display/ui/string_printer.h"
// #include "roo_icons/outlined/18/av.h"
// #include "roo_icons/outlined/24/action.h"
// #include "roo_icons/outlined/24/content.h"
// #include "roo_icons/outlined/24/file.h"

#include "core/include/Lang.h"
#include "core/include/TapLoader.h"
#include "resources/gear_24_00.h"
#include "resources/gear_24_20.h"
#include "resources/gear_24_40.h"
#include "roo_dashboard/meters/percent_progress_bar.h"
#include "roo_icons/outlined/18/navigation.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_icons/outlined/48/av.h"
#include "roo_icons/outlined/48/navigation.h"
#include "roo_windows/fonts/NotoSans_Condensed/11.h"
#include "roo_windows/fonts/NotoSans_Condensed/14.h"
#include "roo_windows/fonts/NotoSans_Condensed/21.h"
#include "roo_windows/fonts/NotoSans_Condensed/28.h"
// #include "roo_toolkit/log/log.h"
#include "roo_windows/containers/aligned_layout.h"
#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/panel.h"
#include "roo_windows/widgets/button.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/text_label.h"

using namespace roo_display;
using namespace roo_windows;

namespace tapuino {

namespace {

class BasePressableButton : public Button {
 protected:
  BasePressableButton(const Environment& env) : Button(env, Button::CONTAINED) {
    setElevation(9, 0);
  }

 public:
  void setPushedDown(bool pushed_down) {
    if (pushed_down == pushed_down_) return;
    pushed_down_ = pushed_down;
    setElevation(pushed_down_ ? 1 : 9, 0);
  }

 private:
  bool pushed_down_;
};

// A simple icon button with a twist that it shows the C64 motor status as an
// additional small icon.
class PlayButton : public BasePressableButton {
 public:
  PlayButton(const Environment& env)
      : BasePressableButton(env), motor_on_(false), frame_idx_(0) {
    setInteriorColor(env.theme().color.primary);
  }

  void paint(const Canvas& canvas) const override {
    Rect bounds = this->bounds();
    MonoIcon play = play_icon();
    play.color_mode().setColor(theme().color.onPrimary);
    if (!motor_on_) {
      canvas.drawTiled(play, bounds, kCenter | kMiddle);
    } else {
      MonoIcon motor = motor_icon();
      motor.color_mode().setColor(theme().color.onPrimary);
      Alignment pause_alignment = (kRight.shiftBy(-5) | kBottom.shiftBy(-5));
      // Find out where the pause icon starts in terms of the X coordinate.
      XDim cutoff =
          pause_alignment.resolveOffset(bounds.asBox(), motor.extents()).dx;
      Canvas play_canvas = canvas;
      play_canvas.clipToExtents(Rect(bounds.xMin(), bounds.yMin(),
                                     bounds.xMin() + cutoff - 1,
                                     bounds.yMax()));
      play_canvas.drawTiled(play, bounds, kCenter | kMiddle);
      Canvas pause_canvas = canvas;
      pause_canvas.clipToExtents(Rect(bounds.xMin() + cutoff, bounds.yMin(),
                                      bounds.xMax(), bounds.yMax()));
      pause_canvas.drawTiled(motor, bounds, pause_alignment);
    }
  }

  Dimensions getSuggestedMinimumDimensions() const override {
    return Dimensions(
        roo_windows::Scaled(4) + play_icon().anchorExtents().width(),
        roo_windows::Scaled(4) + play_icon().anchorExtents().height());
  }

  void setMotorOn(bool motor_on) {
    if (motor_on_ == motor_on) return;
    motor_on_ = motor_on;
    markDirty();
  }

  void tick() {
    ++frame_idx_;
    if (frame_idx_ >= 3) frame_idx_ = 0;
    markDirty();
  }

 private:
  static const MonoIcon& play_icon() { return ic_outlined_48_av_play_arrow(); }

  const MonoIcon& motor_icon() const {
    switch (frame_idx_) {
      case 0:
        return gear_24_00();
      case 1:
        return gear_24_20();
      default:
        return gear_24_40();
    }
  }

  bool motor_on_;
  uint8_t frame_idx_;
};

class StopButton : public BasePressableButton {
 public:
  enum Mode {
    STOP = 0,
    REWIND = 1,
  };

  StopButton(const Environment& env) : BasePressableButton(env), mode_(STOP) {}

  Dimensions getSuggestedMinimumDimensions() const override {
    return Dimensions(
        roo_windows::Scaled(4) + stop_icon().anchorExtents().width(),
        roo_windows::Scaled(4) + stop_icon().anchorExtents().height());
  }

  void paint(const Canvas& canvas) const override {
    MonoIcon icon = mode_ == STOP ? stop_icon() : rewind_icon();
    icon.color_mode().setColor(theme().color.onPrimary);
    canvas.drawTiled(icon, bounds(), kCenter | kMiddle);
  }

  void setMode(Mode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    markDirty();
  }

  Mode mode() const { return mode_; }

 private:
  static const MonoIcon& stop_icon() { return ic_outlined_48_av_stop(); }
  static const MonoIcon& rewind_icon() { return ic_outlined_48_av_replay(); }

  Mode mode_;
};

constexpr roo_windows::YDim RowHeight() {
  return ROO_WINDOWS_ZOOM >= 200   ? 80
         : ROO_WINDOWS_ZOOM >= 150 ? 53
                                   : Scaled(40);
}

inline const roo_display::Font& base_font() {
  return ROO_WINDOWS_ZOOM >= 200   ? roo_display::font_NotoSans_Condensed_28()
         : ROO_WINDOWS_ZOOM >= 150 ? roo_display::font_NotoSans_Condensed_21()
         : ROO_WINDOWS_ZOOM >= 100 ? roo_display::font_NotoSans_Condensed_14()
                                   : roo_display::font_NotoSans_Condensed_11();
}

}  // namespace

class PlayerHeader : public HorizontalLayout {
 public:
  PlayerHeader(const Environment& env, std::function<void()> back_fn)
      : HorizontalLayout(env),
        back_(env, SCALED_ROO_ICON(outlined, navigation_arrow_back)),
        path_(env, "/", base_font(),
              roo_display::kLeft | roo_display::kMiddle) {
    back_.setMargins(MARGIN_NONE);
    path_.setMargins(MARGIN_NONE);
    back_.setPadding(PADDING_TINY, PADDING_SMALL);
    path_.setPadding(PADDING_TINY, PADDING_SMALL);
    add(back_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    add(path_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    setBackground(env.theme().color.secondary);
    back_.setOnInteractiveChange(back_fn);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::ExactHeight(RowHeight()));
  }

  void setPath(std::string path) { path_.setText(std::move(path)); }

 private:
  Icon back_;
  roo_windows::TextLabel path_;
};

class PlayerProgress : public VerticalLayout {
 public:
  PlayerProgress(const Environment& env)
      : VerticalLayout(env),
        progress_bar_(env),
        footer_(env),
        read_bytes_(env, "0", font_body2()),
        all_bytes_(env, "", font_body2(),
                   roo_display::kRight | roo_display::kMiddle) {
    footer_.add(read_bytes_, roo_display::kLeft | roo_display::kTop);
    footer_.add(all_bytes_, roo_display::kRight | roo_display::kTop);
    progress_bar_.setMargins(MARGIN_NONE);
    add(progress_bar_);
    add(footer_);
    read_bytes_.setPadding(PADDING_NONE);
    all_bytes_.setPadding(PADDING_NONE);
    read_bytes_.setMargins(MARGIN_NONE);
    all_bytes_.setMargins(MARGIN_NONE);
    // footer_.setBackground(roo_display::color::PaleGreen);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::WrapContentHeight());
  }

  Margins getMargins() const override {
    return Margins(MARGIN_LARGE, MARGIN_LARGE);
  }

  void setTotalLabel(std::string label) { all_bytes_.setText(label); }

  void setProgress(uint32_t read_bytes, uint16_t progress) {
    progress_bar_.setProgress(progress);
    const char* suffix[] = {"B", "KiB", "MiB", "GiB"};
    uint32_t val = read_bytes;
    uint8_t percent = 0;
    int suffix_idx = 0;
    while (val >= 1024) {
      ++suffix_idx;
      percent = ((uint32_t)100 * (val % 1024)) / 1024;
      val /= 1024;
    }
    if (val >= 100 || suffix_idx == 0) {
      // Print no fractional digits.
      read_bytes_.setTextf("%d %s", val, suffix[suffix_idx]);
    } else if (val >= 10) {
      // Print one fractional digit.
      read_bytes_.setTextf("%d.%d %s", val, percent / 10, suffix[suffix_idx]);
    } else {
      // Print two fractional digits.
      read_bytes_.setTextf("%d.%02d %s", val, percent, suffix[suffix_idx]);
    }
  }

 private:
  roo_dashboard::PercentProgressBar progress_bar_;
  AlignedLayout footer_;
  TextLabel read_bytes_;
  TextLabel all_bytes_;
};

class PlayerContentPanel : public VerticalLayout {
 public:
  PlayerContentPanel(const Environment& env, std::function<void()> back_fn,
                     std::function<void()> play_fn,
                     std::function<void()> stop_fn,
                     std::function<void()> rewind_fn)
      : VerticalLayout(env),
        header_(env, back_fn),
        filename_(env, "", base_font(),
                  roo_display::kCenter | roo_display::kMiddle),
        progress_(env),
        buttons_(env),
        play_(env),
        stop_(env) {
    add(header_);
    add(filename_, VerticalLayout::Params().setWeight(0).setGravity(
                       kHorizontalGravityCenter));
    add(progress_);
    play_.setPadding(PADDING_HUGE, PADDING_TINY);
    play_.setMargins(MARGIN_LARGE, MARGIN_HUMONGOUS);
    stop_.setPadding(PADDING_HUGE, PADDING_TINY);
    stop_.setMargins(MARGIN_LARGE, MARGIN_HUMONGOUS);
    buttons_.add(play_);
    buttons_.add(stop_);
    add(buttons_,
        VerticalLayout::Params().setGravity(kHorizontalGravityCenter));
    stop_.setInteriorColor(roo_display::color::DarkRed);
    play_.setOnInteractiveChange(play_fn);
    // stop_.setEnabled(false);
    play_.setPushedDown(false);
    // stop_.setPushedDown(true);
    stop_.setPushedDown(false);

    stop_.setOnInteractiveChange([stop_fn, rewind_fn, this]() {
      if (stop_.mode() == StopButton::STOP)
        stop_fn();
      else
        rewind_fn();
    });
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::MatchParentHeight());
  }

  void enter(const MemIndexEntry& entry) {
    header_.setPath(entry.parent().getPath());
    filename_.setText(entry.getName());
    char size[16];
    entry.printSize(size);
    progress_.setTotalLabel(size);
  }

  void setPlayStatus(bool is_playing, uint32_t total, uint32_t read,
                     bool motor_on) {
    if (total > 0) {
      uint32_t progress = 1024 * read / total;
      progress_.setProgress(read, progress);
    }
    stop_.setMode(is_playing || read == 0 ? StopButton::STOP
                                          : StopButton::REWIND);
    // bool stop_enabled = is_playing || read > 0;
    // stop_.setPushedDown(!stop_enabled);
    // stop_.setEnabled(stop_enabled);
    play_.setPushedDown(is_playing);
    play_.setEnabled(is_playing || (total == 0 || read < total));
    // If we're not sending the 'sense' signal, Commodore will not enable
    // the motor. So it only makes sense to show 'paused' if we know we
    // are sending 'sense', which is while we're playing.
    play_.setMotorOn(motor_on);
    play_.tick();
  }

 private:
  PlayerHeader header_;
  TextLabel filename_;
  PlayerProgress progress_;
  HorizontalLayout buttons_;
  PlayButton play_;
  StopButton stop_;
};

PlayerActivity::PlayerActivity(const Environment& env,
                               roo_scheduler::Scheduler& scheduler, Sd& sd,
                               MemIndex& mem_index,
                               TapuinoNext::UtilityCollection* utility)
    : scheduler_(scheduler),
      sd_(sd),
      mem_index_(mem_index),
      contents_(nullptr),
      tap_file_(sd),
      shows_playing_(false),
      loader_(utility, scheduler),
      player_status_updater_(
          scheduler, [this]() { updateLoaderStatus(); },
          roo_time::Millis(100)) {
  auto* panel = new PlayerContentPanel(
      env, [&]() { exit(); }, [&]() { play(); }, [&]() { stop(); },
      [&]() { rewind(); });
  contents_.reset(panel);
}

void PlayerActivity::onStart() {}

void PlayerActivity::onStop() {
  mem_index_.clear();
  // SdMount mount(sd_);
  if (sd_.is_mounted()) {
    mem_index_.Load(sd_.fs(), kIndexFilePath);
  }
}

void PlayerActivity::onResume() { player_status_updater_.start(); }

void PlayerActivity::onPause() {
  player_status_updater_.stop();
  loader_.Reset();
  updateLoaderStatus();
}

void PlayerActivity::enter(const MemIndexEntry& entry) {
  tap_file_.set(entry);
  ((PlayerContentPanel&)getContents()).enter(entry);
}

void PlayerActivity::updateLoaderStatus() {
  PlayerContentPanel& contents = (PlayerContentPanel&)getContents();
  const TapuinoNext::TAP_INFO* tap_info = loader_.GetTapInfo();
  if (tap_info->length > 0 && tap_info->position == tap_info->length) {
    // Playback finished; force 'stop'.
    shows_playing_ = false;
  }
  contents.setPlayStatus(shows_playing_, tap_info->length, tap_info->position,
                         digitalRead(C64_MOTOR_PIN));
}

void PlayerActivity::loadFinished(TapuinoNext::ErrorCodes result) {
  shows_playing_ = false;
  switch (result) {
    case TapuinoNext::ErrorCodes::OK: {
      getTask()->showAlertDialog("Loading complete", "Reached the end of file.",
                                 {"OK"}, [this](int) { exit(); });
      break;
    }
    case TapuinoNext::ErrorCodes::OPERATION_ABORTED: {
      // Do nothing. The user knows what they did.
      break;
    }
    default: {
      getTask()->showAlertDialog("Loading failed", TapuinoNext::S_FILE_ERROR,
                                 {"OK"}, [this](int) { exit(); });
    }
  }
}

void PlayerActivity::play() {
  shows_playing_ = true;
  updateLoaderStatus();
  getApplication()->refresh();
  TapuinoNext::ErrorCodes res = loader_.Play(
      tap_file_,
      [this](TapuinoNext::ErrorCodes result) { loadFinished(result); });
  if (res == TapuinoNext::ErrorCodes::OK) {
    // Immediately update the UI without waiting for the periodic update.
    updateLoaderStatus();
  } else {
    shows_playing_ = false;
    getTask()->showAlertDialog(TapuinoNext::S_ERROR, TapuinoNext::S_INVALID_TAP,
                               {"OK"}, [](int) {});
  }
}

void PlayerActivity::stop() { loader_.Stop(); }

void PlayerActivity::rewind() { loader_.Reset(); }

}  // namespace tapuino