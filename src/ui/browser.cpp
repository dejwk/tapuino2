#include "browser.h"

#include "SD.h"
#include "index/mem_index_builder.h"
#include "memory/mem_buffer.h"
#include "roo_display/core/utf8.h"
#include "roo_display/ui/string_printer.h"
#include "roo_material_icons/outlined/24/action.h"
#include "roo_material_icons/outlined/24/content.h"
#include "roo_material_icons/outlined/24/file.h"
#include "roo_material_icons/outlined/24/navigation.h"
#include "roo_smooth_fonts/NotoSans_Condensed/12.h"
#include "roo_smooth_fonts/NotoSans_Condensed/15.h"
#include "roo_smooth_fonts/NotoSans_Condensed/18.h"
// #include "roo_smooth_fonts/NotoSans_CondensedBold/15.h"
#include "roo_toolkit/log/log.h"
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

using namespace roo_windows;

namespace tapuino {

typedef std::function<void(int)> EntrySelectedFn;

class ListEntry : public HorizontalLayout {
 public:
  ListEntry(const Environment& env, EntrySelectedFn select_fn)
      : HorizontalLayout(env),
        icon_(env, ic_outlined_24_file_folder()),
        title_(env, "Foo", roo_display::font_NotoSans_Condensed_15(),
               roo_display::kLeft | roo_display::kMiddle),
        select_fn_(select_fn) {
    icon_.setMargins(MARGIN_NONE);
    icon_.setPadding(PADDING_TINY, PADDING_NONE);
    title_.setMargins(MARGIN_NONE);
    title_.setPadding(PADDING_TINY, PADDING_SMALL);
    add(icon_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    add(title_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
  }

  ListEntry(const ListEntry& other)
      : HorizontalLayout(other),
        icon_(other.icon_),
        title_(other.title_),
        select_fn_(other.select_fn_) {
    add(icon_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    add(title_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::ExactHeight(40));
  }

  void setFolder(int16_t idx, roo_display::StringView name) {
    set(idx, name);
    icon_.setIcon(ic_outlined_24_file_folder());
  }

  void setZip(int16_t idx, roo_display::StringView name) {
    set(idx, name);
    icon_.setIcon(ic_outlined_24_file_folder_open());
  }

  void setFile(int16_t idx, roo_display::StringView name) {
    set(idx, name);
    icon_.setIcon(ic_outlined_24_file_text_snippet());
  }

  bool isClickable() const override { return true; }

  void onClicked() override { select_fn_(idx_); }

 private:
  void set(int16_t idx, roo_display::StringView name) {
    idx_ = idx;
    title_.setText(name);
  }

  Icon icon_;
  TextLabel title_;
  int16_t idx_;
  EntrySelectedFn select_fn_;
};

class BrowserListModel : public ListModel<ListEntry> {
 public:
  BrowserListModel(MemIndex& index)
      : index_(index), content_(nullptr), size_(0) {}

  void update(const MemIndex::PathEntryId* content, int size) {
    content_ = content;
    size_ = size;
  }

  int elementCount() override { return size_; }

  void set(int idx, ListEntry& dest) override {
    MemIndexEntry e(&index_, index_.entry_by_path(content_[idx]));
    if (e.isDir()) {
      dest.setFolder(idx, e.getName());
    } else if (e.isZip()) {
      dest.setZip(idx, e.getName());
    } else if (e.isTapFile()) {
      dest.setFile(idx, e.getName());
    } else {
      dest.setFile(idx, e.getName());
    }
  }

 private:
  MemIndex& index_;
  const MemIndex::PathEntryId* content_;
  int size_;
};

class BrowserHeader : public HorizontalLayout {
 public:
  BrowserHeader(const Environment& env, std::function<void()> back_fn)
      : HorizontalLayout(env),
        back_(env, ic_outlined_24_navigation_arrow_back()),
        path_(env, "/", roo_display::font_NotoSans_Condensed_15(),
              roo_display::kLeft | roo_display::kMiddle) {
    back_.setMargins(MARGIN_NONE);
    path_.setMargins(MARGIN_NONE);
    back_.setPadding(PADDING_TINY, PADDING_SMALL);
    path_.setPadding(PADDING_TINY, PADDING_SMALL);
    add(back_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    add(path_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    setBackground(env.theme().color.secondary);
    back_.setOnClicked(back_fn);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::ExactHeight(40));
  }

  void setPath(std::string path) { path_.setText(std::move(path)); }

 private:
  Icon back_;
  roo_windows::TextLabel path_;
};

class BrowserContentPanel : public VerticalLayout {
 public:
  BrowserContentPanel(const Environment& env, MemIndex& index,
                      EntrySelectedFn select_fn, std::function<void()> back_fn)
      : VerticalLayout(env),
        header_(env, back_fn),
        model_(index),
        content_list_(env, model_, ListEntry(env, select_fn)),
        content_panel_(env, content_list_) {
    add(header_);
    add(content_panel_, VerticalLayout::Params().setWeight(1));
    // setBackground(roo_display::color::LightPink);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::MatchParentHeight());
  }

  void update(bool is_root, std::string path,
              const MemIndex::PathEntryId* content, uint16_t size,
              uint16_t first_pos) {
    header_.setVisibility(is_root ? Widget::GONE : Widget::VISIBLE);
    header_.setPath(std::move(path));
    model_.update(content, size);
    content_list_.modelChanged();
    if (getMainWindow() != nullptr) {
      getMainWindow()->updateLayout();
    }
    content_panel_.scrollTo(0, -40 * first_pos);
  }

 private:
  BrowserHeader header_;
  BrowserListModel model_;
  ListLayout<ListEntry> content_list_;
  ScrollablePanel content_panel_;
};

class FloatingButtons : public VerticalLayout {
 public:
  FloatingButtons(const Environment& env)
      : VerticalLayout(env),
        home_btn_(env, ic_outlined_24_action_home(), Button::TEXT),
        add_btn_(env, ic_outlined_24_content_add(), Button::TEXT),
        search_btn_(env, ic_outlined_24_action_search(), Button::TEXT) {
    add(home_btn_);
    add(add_btn_);
    add(search_btn_);
  }

  uint8_t getElevation() const override { return 10; }
  BorderStyle getBorderStyle() const override { return BorderStyle(10, 0); }

 private:
  SimpleButton home_btn_;
  SimpleButton add_btn_;
  SimpleButton search_btn_;
};

class BrowserPanel : public AlignedLayout {
 public:
  BrowserPanel(const Environment& env, MemIndex& index,
               EntrySelectedFn select_fn, std::function<void()> back_fn)
      : AlignedLayout(env),
        content_(env, index, select_fn, back_fn),
        floating_buttons_(env) {
    // floating_buttons_.add(home_btn_);
    // floating_buttons_.add(add_btn_);
    // floating_buttons_.add(search_btn_);
    // setBackground(roo_display::color::LightGray);
    add(content_, roo_display::kTop | roo_display::kLeft);
    // int h = 40;
    // add(home_btn_, roo_display::kBottom.shiftBy(-24-2*h) |
    // roo_display::kRight.shiftBy(-24)); add(add_btn_,
    // roo_display::kBottom.shiftBy(-24-h) | roo_display::kRight.shiftBy(-24));
    // add(search_btn_, roo_display::kBottom.shiftBy(-24) |
    // roo_display::kRight.shiftBy(-24));
    add(floating_buttons_,
        roo_display::kBottom.shiftBy(-24) | roo_display::kRight.shiftBy(-24));
    // floating_buttons_.setElevation(5, 0);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::MatchParentHeight());
  }

  void update(bool is_root, std::string path,
              const MemIndex::PathEntryId* content, uint16_t size,
              uint16_t first_pos) {
    // home_btn_.setVisibility(is_root ? Widget::GONE : Widget::VISIBLE);
    content_.update(is_root, std::move(path), content, size, first_pos);
  }

 private:
  BrowserContentPanel content_;
  FloatingButtons floating_buttons_;
  // Button home_btn_;
  // NewButton add_btn_;
  // Button search_btn_;
};

BrowsingActivity::BrowsingActivity(const Environment& env,
                                   roo_scheduler::Scheduler& scheduler, Sd& sd,
                                   MemIndex& mem_index,
                                   TapFileSelectFn select_fn)
    : scheduler_(scheduler),
      card_checker_(
          scheduler, [this]() { checkCardPresent(); }, roo_time::Millis(1000)),
      contents_(nullptr),
      sd_(sd),
      mem_index_(mem_index),
      select_fn_(select_fn) {
  auto* panel = new BrowserPanel(
      env, mem_index_, [&](int idx) { onEntryClicked(idx); },
      [&]() { onParentDir(); });
  contents_.reset(panel);
}

void BrowsingActivity::onStart() {
  cd_list_ = (MemIndex::PathEntryId*)membuf::GetWorkBuffer();
  setCwd(0, 0);
}

void BrowsingActivity::onResume() { card_checker_.start(); }

void BrowsingActivity::onPause() { card_checker_.stop(); }

void BrowsingActivity::onStop() {
  // contents_.reset(nullptr);
  // file_index_builder_.reset(nullptr);
}

void BrowsingActivity::checkCardPresent() {
  File f = sd_.fs().open("/");
  if (!f) {
    Serial.printf("The SD card has been removed.");
    sd_.unmount();
    // exit();
    // The app starts fast enough that it's easiest just to restart it. This way
    // we avoid any potential memory fragmentation issues from mounting /
    // unmounting filesystems and allocating / deallocating storate for the
    // in-memory index.
    esp_restart();
  }
  // SdMount mount(sd_);
  // if (!mount.ok()) {
  //   exit();
  // }
}

void BrowsingActivity::setCwd(MemIndex::PathEntryId cd, uint16_t first_pos) {
  cd_ = cd;
  element_count_ = 0;
  MemIndexElementIterator itr(mem_index_, cd);
  MemIndex::PathEntryId i;
  while (itr.next(i)) {
    cd_list_[element_count_++] = i;
  }
  BrowserPanel& p = (BrowserPanel&)getContents();
  p.invalidateInterior();
  MemIndexEntry e(&mem_index_, mem_index_.entry_by_path(cd));
  p.update(e.isRoot(), e.getPath(), cd_list_, element_count_, first_pos);
}

void BrowsingActivity::onEntryClicked(int idx) {
  MemIndexEntry entry(&mem_index_, mem_index_.entry_by_path(cd_list_[idx]));
  if (entry.isContainer()) {
    setCwd(cd_list_[idx], 0);
  } else {
    select_fn_(entry);
  }
}

void BrowsingActivity::onParentDir() {
  MemIndexEntry entry(&mem_index_, mem_index_.entry_by_path(cd_));
  if (entry.isRoot()) return;
  MemIndex::Handle parent_handle = entry.parent_handle();
  MemIndex::PathEntryId cd = cd_;
  uint16_t first_pos = 0;
  while (cd > MemIndex::PathEntryId(0)) {
    --cd;
    MemIndex::Handle h = mem_index_.entry_by_path(cd);
    if (h == parent_handle) {
      setCwd(cd, first_pos);
      return;
    }
    MemIndexEntry e(&mem_index_, h);
    if (e.parent_handle() == parent_handle) {
      ++first_pos;
    }
  }
}

}  // namespace tapuino