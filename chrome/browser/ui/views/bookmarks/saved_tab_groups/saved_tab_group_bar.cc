// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
// The maximum number of buttons (excluding the overflow menu button) that can
// appear in the SavedTabGroupBar.
constexpr int kMaxVisibleButtons = 4;
// The amount of padding between elements listed in the overflow menu.
const int kOverflowMenuButtonPadding = 8;
// The padding at the top and bottom of the bar used to center all displayed
// buttons.
constexpr int kButtonPadding = 4;
// The amount of padding between buttons in the view.
constexpr int kBetweenElementSpacing = 8;
// The thickness, in dips, of the drop indicators during drop sessions.
constexpr int kDropIndicatorThicknessDips = 2;

SavedTabGroupModel* GetSavedTabGroupModelFromBrowser(Browser* browser) {
  DCHECK(browser);
  SavedTabGroupKeyedService* keyed_service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());
  return keyed_service ? keyed_service->model() : nullptr;
}
}  // namespace

// OverflowMenu generally handles drop sessions by delegating to `parent_bar_`.
// Important lifecycle note: when the drop session moves from the bar to the
// overflow menu or vice versa, the state for tracking it in the bar will be
// destroyed and recreated.
class SavedTabGroupBar::OverflowMenu : public views::View {
 public:
  METADATA_HEADER(OverflowMenu);
  explicit OverflowMenu(SavedTabGroupBar& parent_bar)
      : parent_bar_(parent_bar) {}

  ~OverflowMenu() override = default;

  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    return parent_bar_->GetDropFormats(formats, format_types);
  }

  bool AreDropTypesRequired() override {
    return parent_bar_->AreDropTypesRequired();
  }

  bool CanDrop(const OSExchangeData& data) override {
    return parent_bar_->CanDrop(data);
  }

  void OnDragEntered(const ui::DropTargetEvent& event) override {
    parent_bar_->OnDragEntered(event);
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    // Convert the event location into `parent_bar_`'s coordinate space.
    const gfx::Point screen_loc = ConvertPointToScreen(this, event.location());
    const gfx::Point bar_loc =
        ConvertPointFromScreen(base::to_address(parent_bar_), screen_loc);
    ui::DropTargetEvent event_copy(event);
    event_copy.set_location(bar_loc);

    return parent_bar_->OnDragUpdated(event_copy);
  }

  void OnDragExited() override { parent_bar_->OnDragExited(); }

  void OnDragDone() override { parent_bar_->OnDragDone(); }

  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    return parent_bar_->GetDropCallback(event);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    MaybePaintDropIndicatorInOverflow(canvas);
  }

  void MaybePaintDropIndicatorInOverflow(gfx::Canvas* canvas) {
    const absl::optional<int> overflow_menu_indicator_index =
        CalculateDropIndicatorIndexInOverflow();
    if (!overflow_menu_indicator_index.has_value()) {
      return;
    }

    const int y = overflow_menu_indicator_index.value() > 0
                      ? children()[overflow_menu_indicator_index.value() - 1]
                                ->bounds()
                                .bottom() +
                            kOverflowMenuButtonPadding / 2
                      : kDropIndicatorThicknessDips / 2;

    const gfx::Rect drop_indicator_bounds =
        gfx::Rect(0, y - kDropIndicatorThicknessDips / 2, width(),
                  kDropIndicatorThicknessDips);
    canvas->FillRect(drop_indicator_bounds,
                     GetColorProvider()->GetColor(kColorBookmarkBarForeground));
  }

  // Returns the index within the overflow menu the drop indicator should be
  // painted at, or nullopt if no indicator should be painted.
  absl::optional<int> CalculateDropIndicatorIndexInOverflow() {
    const absl::optional<int> indicator_index =
        parent_bar_->CalculateDropIndicatorIndexInCombinedSpace();
    if (!indicator_index.has_value()) {
      return absl::nullopt;
    }

    const int overflow_menu_indicator_index =
        indicator_index.value() - parent_bar_->GetNumberOfVisibleGroups();
    if (overflow_menu_indicator_index < 0) {
      // The drop index is not in the overflow menu. No drop indicator.
      return absl::nullopt;
    }

    const bool came_from_bar =
        parent_bar_->saved_tab_group_model_
            ->GetIndexOf(parent_bar_->drag_data_->guid())
            .value() < parent_bar_->GetNumberOfVisibleGroups();
    if (overflow_menu_indicator_index == 0 && came_from_bar) {
      // The drop index is on the border between the overflow menu and the bar,
      // and because the group came from the bar, it will stay in the bar.
      return absl::nullopt;
    }

    return overflow_menu_indicator_index;
  }

 private:
  // The SavedTabGroupBar that this menu is associated with.
  raw_ref<SavedTabGroupBar> parent_bar_;
};

BEGIN_METADATA(SavedTabGroupBar, OverflowMenu, views::View)
END_METADATA

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   SavedTabGroupModel* saved_tab_group_model,
                                   bool animations_enabled = true)
    : saved_tab_group_model_(saved_tab_group_model),
      browser_(browser),
      animations_enabled_(animations_enabled) {
  // When the #tab-groups-saved feature flag is turned on and profile is
  // regular, `SavedTabGroupBar` is instantiated. If the #tab-groups-saved
  // feature flag is turned off, there is no SavedTabGroupModel.
  DCHECK(base::FeatureList::IsEnabled(features::kTabGroupsSave));
  DCHECK(browser_->profile()->IsRegularProfile());
  DCHECK(saved_tab_group_model_);
  SetAccessibilityProperties(
      ax::mojom::Role::kToolbar,
      /*name=*/l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS));

  SetProperty(views::kElementIdentifierKey, kSavedTabGroupBarElementId);

  // TODO(dljames): Add a container view which only houses the saved buttons.
  // The overflow will continue to be directly added to the bar.
  std::unique_ptr<views::LayoutManager> layout_manager =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(kButtonPadding, 0), kBetweenElementSpacing);
  SetLayoutManager(std::move(layout_manager));

  if (!saved_tab_group_model_) {
    return;
  }

  saved_tab_group_model_->AddObserver(this);

  overflow_button_ = AddChildView(
      std::make_unique<SavedTabGroupOverflowButton>(base::BindRepeating(
          &SavedTabGroupBar::MaybeShowOverflowMenu, base::Unretained(this))));

  HideOverflowButton();
  LoadAllButtonsFromModel();
  ReorderChildView(overflow_button_, children().size());
}

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   bool animations_enabled = true)
    : SavedTabGroupBar(browser,
                       GetSavedTabGroupModelFromBrowser(browser),
                       animations_enabled) {}

SavedTabGroupBar::~SavedTabGroupBar() {
  // Remove all buttons from the hierarchy
  RemoveAllButtons();

  if (saved_tab_group_model_) {
    saved_tab_group_model_->RemoveObserver(this);
  }
}

void SavedTabGroupBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS));
}

void SavedTabGroupBar::UpdateDropIndex() {
  const gfx::Point cursor_location = drag_data_->location().value();
  const base::Uuid dragged_group_guid = drag_data_->guid();

  // Calculates the index in `parent` that a group dragged to `location` should
  // be dropped at. `vertical` should be true when the buttons in `parent` are
  // laid out vertically, and false if they are horizontally laid out.
  // TODO(tbergquist): Use Rect::ManhattanDistanceToPoint instead of `vertical`.
  const auto get_drop_index = [](const views::View* parent, gfx::Point location,
                                 bool vertical) {
    size_t i = 0;
    for (const views::View* const child : parent->children()) {
      const SavedTabGroupButton* const button =
          views::AsViewClass<SavedTabGroupButton>(child);
      // Skip non-button views, or buttons that are not shown.
      if (!button || !button->GetVisible()) {
        continue;
      }

      // We want to drop in front of the first button that's positioned after
      // `cursor_location`.
      if (vertical ? button->bounds().CenterPoint().y() > location.y()
                   : button->bounds().CenterPoint().x() > location.x()) {
        return i;
      }
      i++;
    }
    // If no buttons were after `cursor_location`, drop at the end.
    return i;
  };

  const absl::optional<size_t> current_index =
      saved_tab_group_model_->GetIndexOf(dragged_group_guid);

  absl::optional<size_t> drop_index = absl::nullopt;
  if (overflow_menu_) {
    // `cursor_location` is in mirrored coordinates (i.e. origin in the top
    // right in RTL); ConvertPointFromScreen assumes unmirrored coordinates
    // (i.e. origin in the top left in RTL).
    const gfx::Point unmirrored_loc(GetMirroredXInView(cursor_location.x()),
                                    cursor_location.y());
    const gfx::Point overflow_menu_cursor_loc = ConvertPointFromScreen(
        overflow_menu_, ConvertPointToScreen(this, unmirrored_loc));
    // Re-mirroring is unnecessary, because we only care about y-coordinates
    // after Contains (which wouldn't be affected by re-mirroring anyways).
    if (overflow_menu_->bounds().Contains(overflow_menu_cursor_loc)) {
      drop_index = get_drop_index(overflow_menu_, overflow_menu_cursor_loc,
                                  /* vertical= */ true) +
                   GetNumberOfVisibleGroups();
    }
  }

  if (!drop_index.has_value()) {
    drop_index = get_drop_index(this, cursor_location, false);
  }

  // If the dragged group is going from left to right within the model (i.e. if
  // `desired_index` > `current_index`), we must account for all of the groups
  // between the two indices shifting by 1 when the group is moved from
  // `current_index`.
  if (current_index.has_value() && current_index < drop_index) {
    drop_index = drop_index.value() - 1;
  }

  CHECK_LT(drop_index.value(),
           saved_tab_group_model_->saved_tab_groups().size());
  drag_data_->SetInsertionIndex(drop_index);
  SchedulePaint();
  if (overflow_menu_) {
    overflow_menu_->SchedulePaint();
  }
}

absl::optional<size_t> SavedTabGroupBar::GetDropIndex() const {
  if (!drag_data_ || !drag_data_->insertion_index()) {
    return absl::nullopt;
  }

  CHECK_LT(drag_data_->insertion_index().value(),
           saved_tab_group_model_->saved_tab_groups().size());
  return drag_data_->insertion_index();
}

void SavedTabGroupBar::HandleDrop() {
  saved_tab_group_model_->ReorderGroupLocally(drag_data_->guid(),
                                              GetDropIndex().value());
  drag_data_.release();
  SchedulePaint();
}

bool SavedTabGroupBar::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(SavedTabGroupDragData::GetFormatType());
  return true;
}

bool SavedTabGroupBar::AreDropTypesRequired() {
  return true;
}

bool SavedTabGroupBar::CanDrop(const OSExchangeData& data) {
  absl::optional<SavedTabGroupDragData> drag_data =
      SavedTabGroupDragData::ReadFromOSExchangeData(&data);
  if (!drag_data.has_value()) {
    return false;
  }

  return saved_tab_group_model_->Contains(drag_data.value().guid());
}

void SavedTabGroupBar::OnDragEntered(const ui::DropTargetEvent& event) {
  absl::optional<SavedTabGroupDragData> drag_data =
      SavedTabGroupDragData::ReadFromOSExchangeData(&(event.data()));
  CHECK(drag_data.has_value());

  drag_data_ = std::make_unique<SavedTabGroupDragData>(drag_data.value());

  const int mirrored_x = GetMirroredXInView(event.location().x());
  drag_data_->SetLocation(gfx::Point(mirrored_x, event.location().y()));
  UpdateDropIndex();
}

int SavedTabGroupBar::OnDragUpdated(const ui::DropTargetEvent& event) {
  // Event locations are in unmirrored coordinates (i.e. origin in the top left,
  // even in RTL). Mirror the location so the rest of the calculations can take
  // place in the standard mirrored coordinate space (i.e. origin in the top
  // right in RTL).
  const int mirrored_x = GetMirroredXInView(event.location().x());
  drag_data_->SetLocation(gfx::Point(mirrored_x, event.location().y()));
  UpdateDropIndex();

  const bool dragging_over_button =
      overflow_button_->GetVisible() &&
      mirrored_x >= overflow_button_->bounds().x();
  const bool would_drop_into_overflow =
      GetDropIndex() >= static_cast<size_t>(GetNumberOfVisibleGroups());

  if (dragging_over_button || would_drop_into_overflow) {
    MaybeShowOverflowMenu();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void SavedTabGroupBar::OnDragExited() {
  drag_data_.release();
  SchedulePaint();
}

void SavedTabGroupBar::OnDragDone() {
  drag_data_.release();
  HideOverflowMenu();
  SchedulePaint();
}

views::View::DropCallback SavedTabGroupBar::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(
      [](SavedTabGroupBar* bar, const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& drag,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        bar->HandleDrop();
        drag = ui::mojom::DragOperation::kMove;
      },
      base::Unretained(this));
}

void SavedTabGroupBar::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  MaybePaintDropIndicatorInBar(canvas);
}

void SavedTabGroupBar::SavedTabGroupAddedLocally(const base::Uuid& guid) {
  SavedTabGroupAdded(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedLocally(
    const SavedTabGroup* removed_group) {
  SavedTabGroupRemoved(removed_group->saved_guid());
}

void SavedTabGroupBar::SavedTabGroupLocalIdChanged(
    const base::Uuid& saved_group_id) {
  SavedTabGroupUpdated(saved_group_id);
}

void SavedTabGroupBar::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const absl::optional<base::Uuid>& tab_guid) {
  SavedTabGroupUpdated(group_guid);
}

void SavedTabGroupBar::SavedTabGroupReorderedLocally() {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::SavedTabGroupReorderedFromSync() {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::SavedTabGroupTabsReorderedLocally(
    const base::Uuid& group_guid) {
  SavedTabGroupUpdated(group_guid);
}

void SavedTabGroupBar::SavedTabGroupAddedFromSync(const base::Uuid& guid) {
  SavedTabGroupAdded(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  SavedTabGroupRemoved(removed_group->saved_guid());
}

void SavedTabGroupBar::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const absl::optional<base::Uuid>& tab_guid) {
  SavedTabGroupUpdated(group_guid);
}

void SavedTabGroupBar::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  overflow_menu_ = nullptr;
  bubble_delegate_ = nullptr;
}

void SavedTabGroupBar::Layout() {
  views::View::Layout();
  const bool should_show_overflow = ShouldShowOverflowButtonForWidth(width());
  const int last_visible_button_index =
      CalculateLastVisibleButtonIndexForWidth(width());
  UpdateButtonVisibilities(should_show_overflow, last_visible_button_index);
}

int SavedTabGroupBar::CalculatePreferredWidthRestrictedBy(int max_width) const {
  // Early return if the only button is the overflow button. It should be
  // invisible in this case. Happens when saved tab groups is enabled and no
  // groups are saved yet.
  if (overflow_button_ == children()[0]) {
    return 0;
  }

  // Denotes whether or not the overflow button should be shown. This is true
  // when there are strictly greater than kMaxVisibleButtons buttons OR when the
  // kMaxVisibleButtons buttons do not fit in the space provided.
  const bool should_show_overflow = ShouldShowOverflowButtonForWidth(max_width);
  const int overflow_button_width =
      kBetweenElementSpacing + overflow_button_->GetPreferredSize().width();

  // Reserve space for the overflow button.
  if (should_show_overflow) {
    max_width -= overflow_button_width;
  }

  const int last_visible_button_index =
      CalculateLastVisibleButtonIndexForWidth(max_width);

  int width = should_show_overflow ? overflow_button_width : 0;
  for (int i = 0; i <= last_visible_button_index; ++i) {
    width += children()[i]->GetPreferredSize().width() + kBetweenElementSpacing;
  }

  return width;
}

bool SavedTabGroupBar::IsOverflowButtonVisible() {
  return overflow_button_ && overflow_button_->GetVisible();
}

void SavedTabGroupBar::AddTabGroupButton(const SavedTabGroup& group,
                                         int index) {
  // Check that the index is valid for buttons
  DCHECK_LE(index, static_cast<int>(children().size()));

  AddChildViewAt(
      std::make_unique<SavedTabGroupButton>(
          group,
          base::BindRepeating(&SavedTabGroupBar::page_navigator,
                              base::Unretained(this)),
          base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                              base::Unretained(this), group.saved_guid()),
          browser_, animations_enabled_),
      index);
}

void SavedTabGroupBar::SavedTabGroupAdded(const base::Uuid& guid) {
  absl::optional<int> index = saved_tab_group_model_->GetIndexOf(guid);
  if (!index.has_value()) {
    return;
  }
  AddTabGroupButton(*saved_tab_group_model_->Get(guid), index.value());

  HideOverflowMenu();
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupRemoved(const base::Uuid& guid) {
  RemoveTabGroupButton(guid);

  HideOverflowMenu();
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupUpdated(const base::Uuid& guid) {
  absl::optional<int> index = saved_tab_group_model_->GetIndexOf(guid);
  if (!index.has_value()) {
    return;
  }
  const SavedTabGroup* group = saved_tab_group_model_->Get(guid);
  SavedTabGroupButton* button =
      views::AsViewClass<SavedTabGroupButton>(GetButton(group->saved_guid()));
  DCHECK(button);

  button->UpdateButtonData(*group);

  HideOverflowMenu();
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupReordered() {
  // Selection sort the buttons to match the model's order.
  std::unordered_map<std::string, SavedTabGroupButton*> buttons_by_guid;
  for (views::View* child : children()) {
    SavedTabGroupButton* button =
        views::AsViewClass<SavedTabGroupButton>(child);
    if (button) {
      buttons_by_guid[button->guid().AsLowercaseString()] = button;
    }
  }

  const std::vector<SavedTabGroup>& groups =
      saved_tab_group_model_->saved_tab_groups();
  for (size_t i = 0; i < groups.size(); ++i) {
    const std::string guid = groups[i].saved_guid().AsLowercaseString();
    views::View* const button = buttons_by_guid[guid];
    ReorderChildView(button, i);
  }

  // Ensure the overflow button is the last button in the view hierarchy.
  ReorderChildView(overflow_button_, children().size());

  HideOverflowMenu();
  PreferredSizeChanged();
}

void SavedTabGroupBar::LoadAllButtonsFromModel() {
  const std::vector<SavedTabGroup>& saved_tab_groups =
      saved_tab_group_model_->saved_tab_groups();

  for (size_t index = 0; index < saved_tab_groups.size(); index++) {
    AddTabGroupButton(saved_tab_groups[index], index);
  }
}

void SavedTabGroupBar::RemoveTabGroupButton(const base::Uuid& guid) {
  // Make sure we have a valid button before trying to remove it.
  views::View* button = GetButton(guid);
  DCHECK(button);
  RemoveChildViewT(button);
}

void SavedTabGroupBar::RemoveAllButtons() {
  for (int index = children().size() - 1; index >= 0; index--) {
    RemoveChildViewT(children().at(index));
  }
}

views::View* SavedTabGroupBar::GetButton(const base::Uuid& guid) {
  for (views::View* child : children()) {
    if (views::IsViewClass<SavedTabGroupButton>(child) &&
        views::AsViewClass<SavedTabGroupButton>(child)->guid() == guid) {
      return child;
    }
  }

  return nullptr;
}

void SavedTabGroupBar::OnTabGroupButtonPressed(const base::Uuid& id,
                                               const ui::Event& event) {
  DCHECK(saved_tab_group_model_ && saved_tab_group_model_->Contains(id));
  const SavedTabGroup* group = saved_tab_group_model_->Get(id);

  if (group->saved_tabs().empty()) {
    return;
  }

  bool space_pressed = event.IsKeyEvent() && event.AsKeyEvent()->key_code() ==
                                                 ui::KeyboardCode::VKEY_SPACE;

  bool left_mouse_button_pressed = event.flags() & ui::EF_LEFT_MOUSE_BUTTON;

  if (left_mouse_button_pressed || space_pressed) {
    SavedTabGroupKeyedService* const keyed_service =
        SavedTabGroupServiceFactory::GetForProfile(browser_->profile());

    keyed_service->OpenSavedTabGroupInBrowser(browser_, group->saved_guid());
  }
}

void SavedTabGroupBar::MaybeShowOverflowMenu() {
  // Don't show the menu if it's already showing.
  if (overflow_menu_) {
    return;
  }

  // 1. Build the vertical list of buttons in the over flow menu.
  auto overflow_menu = std::make_unique<OverflowMenu>(*this);
  overflow_menu->SetProperty(views::kElementIdentifierKey,
                             kSavedTabGroupOverflowMenuId);

  // Add all buttons that are not currently visible to the overflow menu.
  for (const auto* const child : children()) {
    if (child->GetVisible() ||
        !views::IsViewClass<SavedTabGroupButton>(child)) {
      continue;
    }

    const SavedTabGroupButton* const button =
        views::AsViewClass<SavedTabGroupButton>(child);
    const SavedTabGroup* const group =
        saved_tab_group_model_->Get(button->guid());

    overflow_menu->AddChildView(std::make_unique<SavedTabGroupButton>(
        *group,
        base::BindRepeating(&SavedTabGroupBar::page_navigator,
                            base::Unretained(this)),
        base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                            base::Unretained(this), group->saved_guid()),
        browser_, animations_enabled_));
  }

  // Make the list of buttons vertical.
  const gfx::Insets insets = gfx::Insets::TLBR(16, 16, 16, 48);
  auto box = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, insets,
      kOverflowMenuButtonPadding);
  box->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  overflow_menu->SetLayoutManager(std::move(box));

  // 2. Create the bubble / background which will hold the overflow menu.
  // TODO(dljames): Set the background color to match the current theme.
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      overflow_button_, views::BubbleBorder::TOP_LEFT);
  bubble_delegate->set_fixed_width(200);
  bubble_delegate->set_margins(gfx::Insets());
  bubble_delegate->set_adjust_if_offscreen(true);
  bubble_delegate->set_close_on_deactivate(true);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetEnableArrowKeyTraversal(true);
  bubble_delegate->SetContentsView(std::move(overflow_menu));

  bubble_delegate_ = bubble_delegate.get();
  overflow_menu_ =
      views::AsViewClass<OverflowMenu>(bubble_delegate->GetContentsView());

  // 3. Display the menu.
  auto* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
  widget_observation_.Observe(widget);
  widget->Show();
}

void SavedTabGroupBar::HideOverflowMenu() {
  if (bubble_delegate_) {
    bubble_delegate_->CancelDialog();
  }
}

void SavedTabGroupBar::HideOverflowButton() {
  overflow_button_->SetVisible(false);
}

void SavedTabGroupBar::ShowOverflowButton() {
  overflow_button_->SetVisible(true);
}

int SavedTabGroupBar::GetNumberOfVisibleGroups() const {
  int count = 0;
  for (const views::View* const child : children()) {
    if (child->GetVisible() && child != overflow_button_) {
      ++count;
    }
  }

  return count;
}

void SavedTabGroupBar::UpdateButtonVisibilities(
    bool show_overflow,
    size_t last_visible_button_index) {
  // Update visibilities
  overflow_button_->SetVisible(show_overflow);
  for (size_t i = 0; i < children().size() - 1; ++i) {
    views::View* button = children()[i];
    button->SetVisible(i <= last_visible_button_index);
  }
}

bool SavedTabGroupBar::ShouldShowOverflowButtonForWidth(int max_width) const {
  const int num_buttons = children().size() - 1;
  return CalculateLastVisibleButtonIndexForWidth(max_width) < num_buttons - 1;
}

int SavedTabGroupBar::CalculateLastVisibleButtonIndexForWidth(
    int max_width) const {
  const int buttons_to_consider =
      std::min(children().size() - 1, size_t(kMaxVisibleButtons));
  int current_width = 0;
  int last_visible_button_index = 0;

  // Only consider buttons from index 0 to kMaxVisibleButtons-1 in the worst
  // case. For each button to consider, check if we have enough room to
  // display it.
  for (int i = 0; i < buttons_to_consider; ++i) {
    const int button_width = children()[i]->GetPreferredSize().width();
    current_width += button_width + kBetweenElementSpacing;

    if (current_width > max_width) {
      break;
    }

    last_visible_button_index = i;
  }

  return last_visible_button_index;
}

void SavedTabGroupBar::MaybePaintDropIndicatorInBar(gfx::Canvas* canvas) {
  const absl::optional<int> indicator_index =
      CalculateDropIndicatorIndexInBar();
  if (!indicator_index.has_value()) {
    return;
  }

  const int x =
      indicator_index.value() > 0
          ? children()[indicator_index.value() - 1]->bounds().right() +
                GetLayoutConstant(BOOKMARK_BAR_BUTTON_PADDING) / 2
          : kDropIndicatorThicknessDips / 2;

  const gfx::Rect drop_indicator_bounds =
      gfx::Rect(x - kDropIndicatorThicknessDips / 2, 0,
                kDropIndicatorThicknessDips, height());
  // `drop_indiciator_bounds` is in mirrored coordinates (i.e. origin in the
  // top right in RTL), but `FillRect` expects unmirrored coordinates (i.e.
  // origin in the top left, even in RTL).
  const gfx::Rect unmirrored_drop_indicator_bounds =
      GetMirroredRect(drop_indicator_bounds);
  canvas->FillRect(unmirrored_drop_indicator_bounds,
                   GetColorProvider()->GetColor(kColorBookmarkBarForeground));
}

absl::optional<int> SavedTabGroupBar::CalculateDropIndicatorIndexInBar() const {
  const absl::optional<int> indicator_index =
      CalculateDropIndicatorIndexInCombinedSpace();
  if (!indicator_index.has_value()) {
    return absl::nullopt;
  }

  if (indicator_index.value() > GetNumberOfVisibleGroups()) {
    // The drop index is not in the bar.
    return absl::nullopt;
  }

  const bool came_from_overflow_menu =
      saved_tab_group_model_->GetIndexOf(drag_data_->guid()).value() >=
      GetNumberOfVisibleGroups();
  if (indicator_index.value() == GetNumberOfVisibleGroups() &&
      came_from_overflow_menu) {
    // The drop index is on the border between the overflow menu and the bar,
    // and because the group came from the overflow menu, it will stay in the
    // overflow menu.
    return absl::nullopt;
  }

  return indicator_index;
}

absl::optional<int>
SavedTabGroupBar::CalculateDropIndicatorIndexInCombinedSpace() const {
  if (!drag_data_ || !GetDropIndex().has_value()) {
    return absl::nullopt;
  }

  const int insertion_index = GetDropIndex().value();
  const int current_index =
      saved_tab_group_model_->GetIndexOf(drag_data_->guid()).value();

  if (insertion_index > current_index) {
    // `insertion_index` doesn't include `current_index`, add it back in if
    // needed.
    return insertion_index + 1;
  } else if (insertion_index == current_index) {
    // Hide the indicator when the drop wouldn't reorder anything.
    return absl::nullopt;
  }

  // Otherwise we can show an indicator at the actual drop index.
  return insertion_index;
}

BEGIN_METADATA(SavedTabGroupBar, views::AccessiblePaneView)
END_METADATA
