// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"

namespace {

bool IsDesktopMediaTabList(views::View* view) {
  return !strcmp(view->GetClassName(), "DesktopMediaTabList");
}

}  // namespace

DesktopMediaPickerViewsTestApi::DesktopMediaPickerViewsTestApi() = default;
DesktopMediaPickerViewsTestApi::~DesktopMediaPickerViewsTestApi() = default;

void DesktopMediaPickerViewsTestApi::FocusSourceAtIndex(size_t index,
                                                        bool select) {
  views::View* source_view = GetSourceAtIndex(index);
  if (source_view) {
    source_view->RequestFocus();
  } else {
    GetTableView()->RequestFocus();
    if (select)
      GetTableView()->Select(index);
  }
}

bool DesktopMediaPickerViewsTestApi::AudioSupported(
    DesktopMediaList::Type type) const {
  return DesktopMediaPickerDialogView::AudioSupported(type);
}

void DesktopMediaPickerViewsTestApi::FocusAudioCheckbox() {
  picker_->dialog_->audio_share_checkbox_->RequestFocus();
}

void DesktopMediaPickerViewsTestApi::PressMouseOnSourceAtIndex(
    size_t index,
    bool double_click) {
  int flags = ui::EF_LEFT_MOUSE_BUTTON;
  if (double_click)
    flags |= ui::EF_IS_DOUBLE_CLICK;
  views::View* source_view = GetSourceAtIndex(index);
  if (source_view) {
    source_view->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), flags, ui::EF_LEFT_MOUSE_BUTTON));
  } else {
    // There's no source view to target, and trying to target a specific source
    // within a larger view would be breakage-prone; just ask the TableView to
    // to select.
    GetTableView()->Select(index);
    if (double_click) {
      GetTableView()->observer()->OnDoubleClick();
    }
  }
}

void DesktopMediaPickerViewsTestApi::PressKeyOnSourceAtIndex(
    size_t index,
    const ui::KeyEvent& event) {
  views::View* source_view = GetSourceAtIndex(index);
  if (source_view) {
    source_view->OnKeyPressed(event);
  } else {
    // TableView rows don't receive key events directly; just send the key event
    // to the TableView itself.
    GetTableView()->OnKeyPressed(event);
  }
}

void DesktopMediaPickerViewsTestApi::DoubleTapSourceAtIndex(size_t index) {
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
  details.set_tap_count(2);
  ui::GestureEvent double_tap(10, 10, 0, base::TimeTicks(), details);
  views::View* source_view = GetSourceAtIndex(index);
  DCHECK(source_view);
  source_view->OnGestureEvent(&double_tap);
}

void DesktopMediaPickerViewsTestApi::SelectTabForSourceType(
    DesktopMediaList::Type source_type) {
  const auto& categories = picker_->dialog_->categories_;
  const auto i = base::ranges::find(
      categories, source_type,
      &DesktopMediaPickerDialogView::DisplaySurfaceCategory::type);
  DCHECK(i != categories.cend());
  if (picker_->dialog_->tabbed_pane_) {
    picker_->dialog_->tabbed_pane_->SelectTabAt(
        std::distance(categories.cbegin(), i));
  }
}

DesktopMediaList::Type
DesktopMediaPickerViewsTestApi::GetSelectedSourceListType() const {
  return picker_->dialog_->GetSelectedSourceListType();
}

absl::optional<int> DesktopMediaPickerViewsTestApi::GetSelectedSourceId()
    const {
  DesktopMediaListController* controller =
      picker_->dialog_->GetSelectedController();
  absl::optional<content::DesktopMediaID> source = controller->GetSelection();
  return source.has_value() ? absl::optional<int>(source.value().id)
                            : absl::nullopt;
}

bool DesktopMediaPickerViewsTestApi::HasSourceAtIndex(size_t index) const {
  const views::TableView* table = GetTableView();
  if (table)
    return base::checked_cast<size_t>(table->GetRowCount()) > index;
  return !!GetSourceAtIndex(index);
}

views::View* DesktopMediaPickerViewsTestApi::GetSelectedListView() {
  return picker_->dialog_->GetSelectedController()->view_;
}

DesktopMediaListController*
DesktopMediaPickerViewsTestApi::GetSelectedController() {
  return picker_->dialog_->GetSelectedController();
}

views::Checkbox* DesktopMediaPickerViewsTestApi::GetAudioShareCheckbox() {
  return picker_->dialog_->audio_share_checkbox_;
}

views::MdTextButton* DesktopMediaPickerViewsTestApi::GetReselectButton() {
  return picker_->dialog_->reselect_button_;
}

const views::View* DesktopMediaPickerViewsTestApi::GetSourceAtIndex(
    size_t index) const {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  if (IsDesktopMediaTabList(list) || index >= list->children().size())
    return nullptr;
  return list->children()[index];
}

views::View* DesktopMediaPickerViewsTestApi::GetSourceAtIndex(size_t index) {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  if (IsDesktopMediaTabList(list) || index >= list->children().size())
    return nullptr;
  return list->children()[index];
}

const views::TableView* DesktopMediaPickerViewsTestApi::GetTableView() const {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  return IsDesktopMediaTabList(list)
             ? static_cast<DesktopMediaTabList*>(list)->list_.get()
             : nullptr;
}

views::TableView* DesktopMediaPickerViewsTestApi::GetTableView() {
  views::View* list = picker_->dialog_->GetSelectedController()->view_;
  return IsDesktopMediaTabList(list)
             ? static_cast<DesktopMediaTabList*>(list)->list_.get()
             : nullptr;
}
