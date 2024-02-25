#include "browser.h"

#include <limits>

#include "SD.h"
#include "index/mem_index_builder.h"
#include "memory/mem_buffer.h"
#include "roo_display/core/utf8.h"
#include "roo_display/ui/string_printer.h"
#include "roo_icons/outlined/24/action.h"
#include "roo_icons/outlined/24/content.h"
#include "roo_icons/outlined/24/file.h"
#include "roo_icons/outlined/24/navigation.h"
#include "roo_icons/outlined/36/action.h"
#include "roo_icons/outlined/36/content.h"
#include "roo_icons/outlined/36/file.h"
#include "roo_icons/outlined/36/navigation.h"
#include "roo_smooth_fonts/NotoSans_Bold/15.h"
#include "roo_smooth_fonts/NotoSans_Condensed/15.h"
#include "roo_smooth_fonts/NotoSans_Regular/15.h"
#include "roo_windows/config.h"
#include "roo_windows/containers/aligned_layout.h"
#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/list_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/application.h"
#include "roo_windows/core/panel.h"
#include "roo_windows/fonts/NotoSans_Condensed/11.h"
#include "roo_windows/fonts/NotoSans_Condensed/14.h"
#include "roo_windows/fonts/NotoSans_Condensed/21.h"
#include "roo_windows/fonts/NotoSans_Condensed/28.h"
#include "roo_windows/widgets/button.h"
#include "roo_windows/widgets/icon.h"
#include "roo_windows/widgets/text_label.h"

using namespace roo_windows;

namespace tapuino {

extern const char* kIndexDir;
extern const char* kMasterIndex;
extern const char* kMemIndex;
extern const char* kMasterIndexTmp;
extern const char* kMemIndexTmp;

namespace {

constexpr roo_windows::YDim RowHeight() {
  return ROO_WINDOWS_ZOOM >= 200   ? 80
         : ROO_WINDOWS_ZOOM >= 150 ? 53
                                   : Scaled(40);
}

inline const roo_display::Font& base_font() {
  return roo_display::font_NotoSans_Condensed_14();
  //  return roo_display::font_NotoSans_Regular_15();
  // return ROO_WINDOWS_ZOOM >= 200
  //            ? roo_display::font_RobotoCondensed_Regular_28()
  //        : ROO_WINDOWS_ZOOM >= 150
  //            ? roo_display::font_RobotoCondensed_Regular_21()
  //        : ROO_WINDOWS_ZOOM >= 100
  //            ? roo_display::font_RobotoCondensed_Regular_14()
  //            : roo_display::font_RobotoCondensed_Regular_11();
}

}  // namespace

typedef std::function<void(int)> EntrySelectedFn;
typedef std::function<void()> PanelScrolledFn;

class ListEntry : public HorizontalLayout {
 public:
  ListEntry(const Environment& env, EntrySelectedFn select_fn,
            EntrySelectedFn mark_fn)
      : HorizontalLayout(env),
        icon_(env, SCALED_ROO_ICON(outlined, file_folder)),
        title_(env, "Foo", base_font(),
               roo_display::kLeft | roo_display::kMiddle),
        select_fn_(select_fn),
        mark_fn_(mark_fn),
        is_readonly_(true) {
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
        select_fn_(other.select_fn_),
        mark_fn_(other.mark_fn_),
        is_readonly_(other.is_readonly_) {
    add(icon_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    add(title_, HorizontalLayout::Params().setGravity(kVerticalGravityMiddle));
    setSelected(false);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::ExactHeight(RowHeight()));
  }

  void setFolder(int16_t idx, roo_display::StringView name, bool selected,
                 bool is_readonly) {
    set(idx, name, selected, is_readonly);
    icon_.setIcon(SCALED_ROO_ICON(outlined, file_folder));
  }

  void setZip(int16_t idx, roo_display::StringView name, bool selected,
              bool is_readonly) {
    set(idx, name, selected, is_readonly);
    icon_.setIcon(SCALED_ROO_ICON(outlined, file_folder_open));
  }

  void setFile(int16_t idx, roo_display::StringView name, bool selected,
               bool is_readonly) {
    set(idx, name, selected, is_readonly);
    icon_.setIcon(SCALED_ROO_ICON(outlined, file_text_snippet));
  }

  bool isClickable() const override { return true; }

  void onClicked() override { select_fn_(idx_); }

  bool supportsLongPress() override { return !is_readonly_; }

  void onLongPress(XDim x, YDim y) override { mark_fn_(idx_); }

 private:
  void set(int16_t idx, roo_display::StringView name, bool selected,
           bool is_readonly) {
    idx_ = idx;
    title_.setText(name);
    setSelected(selected);
    is_readonly_ = is_readonly;
  }

  Icon icon_;
  TextLabel title_;
  int16_t idx_;
  EntrySelectedFn select_fn_;
  EntrySelectedFn mark_fn_;
  bool is_readonly_;
};

class BrowserListModel : public ListModel<ListEntry> {
 public:
  BrowserListModel(MemIndex& index)
      : index_(index),
        content_(nullptr),
        size_(0),
        is_readonly_(false),
        selected_(-1) {}

  void update(const MemIndex::PathEntryId* content, int size,
              bool is_readonly) {
    content_ = content;
    size_ = size;
    is_readonly_ = is_readonly;
    selected_ = -1;
  }

  void select(int idx) {
    // if (idx == selected_) return;
    selected_ = idx;
  }

  int selected() const { return selected_; }

  int elementCount() override { return size_; }

  void set(int idx, ListEntry& dest) override {
    MemIndexEntry e(&index_, index_.entry_by_path(content_[idx]));
    if (e.isDir()) {
      dest.setFolder(idx, e.getName(), idx == selected_, is_readonly_);
    } else if (e.isZip()) {
      dest.setZip(idx, e.getName(), idx == selected_, is_readonly_);
    } else if (e.isTapFile()) {
      dest.setFile(idx, e.getName(), idx == selected_, is_readonly_);
    } else {
      dest.setFile(idx, e.getName(), idx == selected_, is_readonly_);
    }
  }

 private:
  MemIndex& index_;
  const MemIndex::PathEntryId* content_;
  int size_;
  bool is_readonly_;

  // The row that is marked (selected) for further operations (deletion, rename,
  // etc.)
  int selected_;
};

class BrowserHeader : public HorizontalLayout {
 public:
  BrowserHeader(const Environment& env, std::function<void()> back_fn)
      : HorizontalLayout(env),
        back_(env, SCALED_ROO_ICON(outlined, navigation_arrow_back)),
        path_(env, "/", base_font(), roo_display::kLeft | roo_display::kMiddle),
        is_elevated_(false) {
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

  void setElevated(bool elevated) {
    if (is_elevated_ == elevated) return;
    LOG(INFO) << "Setting is_elevated_ to " << elevated;
    is_elevated_ = elevated;
    elevationChanged(4);
  }

  uint8_t getElevation() const override { return is_elevated_ ? 4 : 0; }

  void setPath(std::string path) { path_.setText(std::move(path)); }

 private:
  Icon back_;
  roo_windows::TextLabel path_;
  bool is_elevated_;
};

class ScrollableContent : public ScrollablePanel {
 public:
  ScrollableContent(const Environment& env, WidgetRef contents,
                    std::function<void()> scrolled_cb)
      : ScrollablePanel(env, std::move(contents)),
        scrolled_cb_(std::move(scrolled_cb)) {}

  void onScrollPositionChanged() override { scrolled_cb_(); }

 private:
  std::function<void()> scrolled_cb_;
};

class BrowserContentPanel : public VerticalLayout {
 public:
  BrowserContentPanel(const Environment& env, MemIndex& index,
                      EntrySelectedFn select_fn, std::function<void()> back_fn,
                      std::function<void()> scrolled_cb,
                      EntrySelectedFn mark_fn)
      : VerticalLayout(env),
        header_(env, back_fn),
        model_(index),
        content_list_(env, model_, ListEntry(env, select_fn, mark_fn)),
        content_panel_(env, content_list_, std::move(scrolled_cb)) {
    add(header_);
    add(content_panel_, VerticalLayout::Params().setWeight(1));
    // setBackground(roo_display::color::LightPink);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::MatchParentHeight());
  }

  Widget& getChild(int idx) override {
    switch (idx) {
      case 0:
        return content_panel_;
      case 1:
      default:
        return header_;
    }
  }

  void update(bool is_root, bool is_readonly, std::string path,
              const MemIndex::PathEntryId* content, uint16_t size) {
    header_.setVisibility(is_root ? Widget::GONE : Widget::VISIBLE);
    header_.setPath(std::move(path));
    model_.update(content, size, is_readonly);
    content_list_.modelChanged();
    if (getMainWindow() != nullptr) {
      getMainWindow()->updateLayout();
    }
    content_panel_.setVerticalScrollBarPresence(
        VerticalScrollBar::SHOWN_WHEN_SCROLLING);
  }

  void scrollToPos(uint16_t first_pos) {
    scrollToDim(-RowHeight() * first_pos);
  }

  void scrollToDim(YDim scroll_pos) {
    content_panel_.scrollTo(0, scroll_pos);
    header_.setElevated(scroll_pos < 0);
  }

  void notifyScrolled() {
    select(-1);
    header_.setElevated(content_panel_.getScrollPosition().y < 0);
  }

  void select(int idx) {
    if (idx == model_.selected()) return;
    int prev_selected = model_.selected();
    model_.select(idx);
    if (prev_selected >= 0) {
      content_list_.modelItemChanged(prev_selected);
    }
    if (idx >= 0) {
      content_list_.modelItemChanged(idx);
    }
  }

  int selected() const { return model_.selected(); }

 private:
  BrowserHeader header_;
  BrowserListModel model_;
  ListLayout<ListEntry> content_list_;
  ScrollableContent content_panel_;
};

class FloatingButtons : public VerticalLayout {
 public:
  struct Callbacks {
    std::function<void()> unfold;
    std::function<void()> home;
    std::function<void()> clear;
    std::function<void()> del;
  };

  FloatingButtons(const Environment& env, Callbacks callbacks)
      : VerticalLayout(env),
        unfold_btn_(env, SCALED_ROO_ICON(outlined, navigation_menu),
                    Button::TEXT),
        home_btn_(env, SCALED_ROO_ICON(outlined, action_home), Button::TEXT),
        search_btn_(env, SCALED_ROO_ICON(outlined, action_search),
                    Button::TEXT),
        add_btn_(env, SCALED_ROO_ICON(outlined, content_add), Button::TEXT),
        delete_btn_(env, SCALED_ROO_ICON(outlined, action_delete),
                    Button::TEXT),
        rename_btn_(env,
                    SCALED_ROO_ICON(outlined, file_drive_file_rename_outline),
                    Button::TEXT),
        clear_btn_(env, SCALED_ROO_ICON(outlined, content_clear),
                   Button::TEXT) {
    add(unfold_btn_);
    add(home_btn_);
    add(search_btn_);
    add(add_btn_);
    add(delete_btn_);
    add(rename_btn_);
    add(clear_btn_);

    unfold_btn_.setOnInteractiveChange(std::move(callbacks.unfold));
    home_btn_.setOnInteractiveChange(std::move(callbacks.home));
    clear_btn_.setOnInteractiveChange(std::move(callbacks.clear));
    delete_btn_.setOnInteractiveChange(std::move(callbacks.del));

    add_btn_.setEnabled(false);
    search_btn_.setEnabled(false);
    rename_btn_.setEnabled(false);

    setBrowse(true, false);
  }

  uint8_t getElevation() const override { return 10; }

  BorderStyle getBorderStyle() const override {
    return BorderStyle(Scaled(10), 0);
  }

  void setBrowse(bool is_root, bool is_readonly) {
    unfold_btn_.setVisibility(GONE);
    home_btn_.setVisibility(is_root ? GONE : VISIBLE);
    search_btn_.setVisibility(VISIBLE);
    add_btn_.setVisibility(is_readonly ? GONE : VISIBLE);
    delete_btn_.setVisibility(GONE);
    rename_btn_.setVisibility(GONE);
    clear_btn_.setVisibility(GONE);
  }

  void fold() {
    unfold_btn_.setVisibility(VISIBLE);
    home_btn_.setVisibility(GONE);
    search_btn_.setVisibility(GONE);
    add_btn_.setVisibility(GONE);
    delete_btn_.setVisibility(GONE);
    rename_btn_.setVisibility(GONE);
    clear_btn_.setVisibility(GONE);
  }

  void setEdit() {
    unfold_btn_.setVisibility(GONE);
    home_btn_.setVisibility(GONE);
    search_btn_.setVisibility(GONE);
    add_btn_.setVisibility(GONE);
    delete_btn_.setVisibility(VISIBLE);
    rename_btn_.setVisibility(VISIBLE);
    clear_btn_.setVisibility(VISIBLE);
  }

 private:
  SimpleButton unfold_btn_;
  SimpleButton home_btn_;
  SimpleButton search_btn_;
  SimpleButton add_btn_;
  SimpleButton delete_btn_;
  SimpleButton rename_btn_;
  SimpleButton clear_btn_;
};

class BrowserPanel : public AlignedLayout {
 public:
  BrowserPanel(const Environment& env, MemIndex& index,
               EntrySelectedFn select_fn, std::function<void()> back_fn,
               std::function<void()> home_fn, EntrySelectedFn del_fn)
      : AlignedLayout(env),
        content_(
            env, index, select_fn, back_fn, [this]() { notifyScrolled(); },
            [this](int pos) { notifyMarked(pos); }),
        floating_buttons_(
            env, FloatingButtons::Callbacks{
                     .unfold = [&]() { unfoldMenuClicked(); },
                     .home = std::move(home_fn),
                     .clear = [&]() { clearClicked(); },
                     .del = [this, del_fn]() { del_fn(content_.selected()); }}),
        is_root_(false) {
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
    add(floating_buttons_, roo_display::kBottom.shiftBy(Scaled(-30)) |
                               roo_display::kRight.shiftBy(Scaled(-30)));
    // floating_buttons_.setElevation(5, 0);
  }

  PreferredSize getPreferredSize() const override {
    return PreferredSize(PreferredSize::MatchParentWidth(),
                         PreferredSize::MatchParentHeight());
  }

  void update(bool is_root, bool is_readonly, std::string path,
              const MemIndex::PathEntryId* content, uint16_t size) {
    is_root_ = is_root;
    is_readonly_ = is_readonly;
    // home_btn_.setVisibility(is_root ? Widget::GONE : Widget::VISIBLE);
    content_.update(is_root, is_readonly, std::move(path), content, size);
    floating_buttons_.setBrowse(is_root_, is_readonly_);
  }

  void scrollToPos(uint16_t first_pos) { content_.scrollToPos(first_pos); }

  void scrollToDim(YDim scroll_pos) { content_.scrollToDim(scroll_pos); }

  void notifyScrolled() {
    content_.notifyScrolled();
    floating_buttons_.fold();
  }

  void notifyMarked(int pos) {
    content_.select(pos);
    floating_buttons_.setEdit();
  }

  void unfoldMenuClicked() {
    floating_buttons_.setBrowse(is_root_, is_readonly_);
  }

  void clearClicked() {
    content_.select(-1);
    floating_buttons_.setBrowse(is_root_, is_readonly_);
  }

 private:
  BrowserContentPanel content_;
  FloatingButtons floating_buttons_;
  bool is_root_;
  bool is_readonly_;
  // Button home_btn_;
  // NewButton add_btn_;
  // Button search_btn_;
};

BrowsingActivity::BrowsingActivity(const Environment& env,
                                   roo_scheduler::Scheduler& scheduler, Sd& sd,
                                   Catalog& catalog, TapFileSelectFn select_fn)
    : scheduler_(scheduler),
      card_checker_(
          scheduler, [this]() { checkCardPresent(); }, roo_time::Millis(1000)),
      contents_(nullptr),
      sd_(sd),
      catalog_(catalog),
      select_fn_(select_fn) {
  auto* panel = new BrowserPanel(
      env, catalog_.mem_index(), [&](int idx) { onEntryClicked(idx); },
      [&]() { onParentDir(); }, [&]() { onHomeClicked(); },
      [&](int idx) { onFileDeleted(idx); });
  contents_.reset(panel);
}

void BrowsingActivity::onStart() {
  cd_list_ = (MemIndex::PathEntryId*)membuf::GetWorkBuffer();
  setCwd(0);
  scrollToDim(0);
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

void BrowsingActivity::setCwd(MemIndex::PathEntryId cd) {
  cd_ = cd;
  // Populate the cd_list_ and element_count_ by iterating over the directory
  // contents.
  element_count_ = 0;
  MemIndexElementIterator itr(catalog_.mem_index(), cd);
  MemIndex::PathEntryId i;
  while (itr.next(i)) {
    cd_list_[element_count_++] = i;
  }
  BrowserPanel& p = (BrowserPanel&)getContents();
  p.invalidateInterior();
  MemIndexEntry e = catalog_.resolve(cd);
  p.update(e.isRoot(), e.isContainerReadOnly(), e.getPath(), cd_list_,
           element_count_);
}

void BrowsingActivity::scrollToPos(uint16_t pos) {
  ((BrowserPanel&)getContents()).scrollToPos(pos);
}

void BrowsingActivity::scrollToDim(YDim y) {
  ((BrowserPanel&)getContents()).scrollToDim(y);
}

void BrowsingActivity::onEntryClicked(int idx) {
  MemIndexEntry entry(&catalog_.mem_index(),
                      catalog_.mem_index().entry_by_path(cd_list_[idx]));
  if (entry.isContainer()) {
    setCwd(cd_list_[idx]);
    scrollToDim(0);
  } else {
    select_fn_(entry);
  }
}

void BrowsingActivity::onParentDir() {
  Catalog::PositionInParent pos = catalog_.getPositionInParent(cd_);
  setCwd(pos.parent);
  scrollToPos(pos.position);
}

void BrowsingActivity::onHomeClicked() {
  setCwd(0);
  scrollToDim(0);
}

void BrowsingActivity::onFileDeleted(int idx) {
  MemIndexEntry entry(&catalog_.mem_index(),
                      catalog_.mem_index().entry_by_path(cd_list_[idx]));
  getTask()->showAlertDialog(
      "Confirm delete",
      roo_display::StringPrintf("The following %s:\n%s\nwill be permanently "
                                "deleted.\nAre you sure?",
                                entry.isDir() ? "directory" : "file",
                                entry.getPath().c_str()),
      {"CANCEL", "OK"}, [this, idx](int pos) {
        if (pos == 1) onFileDeletedConfirmed(idx);
      });
}

std::string BrowsingActivity::currentPath() const {
  MemIndexEntry self(&catalog_.mem_index(),
                     catalog_.mem_index().entry_by_path(cd_));
  return self.getPath();
}

void BrowsingActivity::onFileDeletedConfirmed(int idx) {
  getTask()->showAlertDialog("Please wait", "Deleting ...", {}, nullptr);
  getApplication()->refresh();
  bool ok = catalog_.deletePath(cd_list_[idx]);

  // Since anything we deleted must be a descentant of cd_, and thus, after cd_
  // in the index sorted by path, the cd_ did not change because of the
  // deletion. But we need to setCwd in order to refresh the contents.
  setCwd(cd_);
  getTask()->clearDialog();
  if (!ok) {
    getTask()->showAlertDialog("Error", "Delete failed.", {"OK"}, nullptr);
  }
}

}  // namespace tapuino