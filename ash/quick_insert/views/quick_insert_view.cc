// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_view.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/quick_insert/metrics/quick_insert_performance_metrics.h"
#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/model/quick_insert_caps_lock_position.h"
#include "ash/quick_insert/model/quick_insert_mode_type.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view.h"
#include "ash/quick_insert/views/quick_insert_item_with_submenu_view.h"
#include "ash/quick_insert/views/quick_insert_key_event_handler.h"
#include "ash/quick_insert/views/quick_insert_main_container_view.h"
#include "ash/quick_insert/views/quick_insert_page_view.h"
#include "ash/quick_insert/views/quick_insert_positioning.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_search_bar_textfield.h"
#include "ash/quick_insert/views/quick_insert_search_field_view.h"
#include "ash/quick_insert/views/quick_insert_search_results_view.h"
#include "ash/quick_insert/views/quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_strings.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/quick_insert/views/quick_insert_submenu_view.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_zero_state_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr int kVerticalPaddingBetweenPickerContainers = 8;

// Padding to separate the Quick Insert window from the screen edge.
constexpr gfx::Insets kPaddingFromScreenEdge(16);

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_SHADOW);
  border->SetCornerRadius(kQuickInsertContainerBorderRadius);
  border->SetColor(SK_ColorTRANSPARENT);
  return border;
}

// Gets the preferred Quick Insert view bounds in screen coordinates. We try to
// place the Quick Insert view close to `anchor_bounds`, while taking into
// account `layout_type`, `quick_insert_view_size` and available space on the
// screen. `quick_insert_view_search_field_vertical_offset` is the vertical
// offset from the top of the Quick Insert view to the center of the search
// field, which we use to try to vertically align the search field with the
// center of the anchor bounds. `anchor_bounds` and returned bounds should be in
// screen coordinates.
gfx::Rect GetQuickInsertViewBoundsWithoutSelectedText(
    const gfx::Rect& anchor_bounds,
    PickerLayoutType layout_type,
    const gfx::Size& quick_insert_view_size,
    int quick_insert_view_search_field_vertical_offset) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(anchor_bounds)
                                   .work_area();
  screen_work_area.Inset(kPaddingFromScreenEdge);
  gfx::Rect quick_insert_view_bounds(quick_insert_view_size);
  if (anchor_bounds.right() + quick_insert_view_size.width() <=
      screen_work_area.right()) {
    // If there is space, place Quick Insert to the right of the anchor,
    // vertically aligning the center of the Quick Insert search field with the
    // center of the anchor.
    quick_insert_view_bounds.set_origin(anchor_bounds.right_center());
    quick_insert_view_bounds.Offset(
        0, -quick_insert_view_search_field_vertical_offset);
  } else {
    switch (layout_type) {
      case PickerLayoutType::kMainResultsBelowSearchField:
        // Try to place Quick Insert at the right edge of the screen, below
        // the anchor.
        quick_insert_view_bounds.set_origin(
            {screen_work_area.right() - quick_insert_view_size.width(),
             anchor_bounds.bottom()});
        break;
      case PickerLayoutType::kMainResultsAboveSearchField:
        // Try to place Quick Insert at the right edge of the screen, above
        // the anchor.
        quick_insert_view_bounds.set_origin(
            {screen_work_area.right() - quick_insert_view_size.width(),
             anchor_bounds.y() - quick_insert_view_size.height()});
        break;
    }
  }

  // Adjust if necessary to keep the whole Quick Insert view onscreen. Note that
  // the non client area of Quick Insert, e.g. the shadows, are allowed to
  // be offscreen.
  quick_insert_view_bounds.AdjustToFit(screen_work_area);
  return quick_insert_view_bounds;
}

// Gets the preferred Quick Insert view bounds in the case that there is
// selected text. We try to left align the Quick Insert view above or below
// `anchor_bounds`, while taking into account `layout_type`,
// `quick_insert_view_size` and available space on the screen. `anchor_bounds`
// and returned bounds should be in screen coordinates.
gfx::Rect GetQuickInsertViewBoundsWithSelectedText(
    const gfx::Rect& anchor_bounds,
    PickerLayoutType layout_type,
    const gfx::Size& quick_insert_view_size) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(anchor_bounds)
                                   .work_area();
  screen_work_area.Inset(kPaddingFromScreenEdge);
  gfx::Rect quick_insert_view_bounds(quick_insert_view_size);
  switch (layout_type) {
    case PickerLayoutType::kMainResultsBelowSearchField:
      // Left aligned below the anchor.
      quick_insert_view_bounds.set_origin(
          gfx::Point(anchor_bounds.x(), anchor_bounds.bottom()));
      break;
    case PickerLayoutType::kMainResultsAboveSearchField:
      // Left aligned above the anchor.
      quick_insert_view_bounds.set_origin(
          gfx::Point(anchor_bounds.x(),
                     anchor_bounds.y() - quick_insert_view_size.height()));
      break;
  }

  // Adjust if necessary to keep the whole Quick Insert view onscreen.
  quick_insert_view_bounds.AdjustToFit(screen_work_area);
  return quick_insert_view_bounds;
}

QuickInsertCategory GetCategoryForMoreResults(QuickInsertSectionType type) {
  switch (type) {
    case QuickInsertSectionType::kNone:
    case QuickInsertSectionType::kContentEditor:
    case QuickInsertSectionType::kExamples:
      NOTREACHED();
    case QuickInsertSectionType::kClipboard:
      return QuickInsertCategory::kClipboard;
    case QuickInsertSectionType::kLinks:
      return QuickInsertCategory::kLinks;
    case QuickInsertSectionType::kLocalFiles:
      return QuickInsertCategory::kLocalFiles;
    case QuickInsertSectionType::kDriveFiles:
      return QuickInsertCategory::kDriveFiles;
  }
}

std::u16string GetSearchFieldPlaceholderText(PickerModeType mode,
                                             bool is_editor_available) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (mode) {
    case PickerModeType::kUnfocused:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SEARCH_FIELD_NO_FOCUS_PLACEHOLDER_TEXT);
    case PickerModeType::kNoSelection:
      return l10n_util::GetStringUTF16(
          is_editor_available
              ? IDS_PICKER_SEARCH_FIELD_NO_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT
              : IDS_PICKER_SEARCH_FIELD_NO_SELECTION_PLACEHOLDER_TEXT);
    case PickerModeType::kHasSelection:
      return l10n_util::GetStringUTF16(
          is_editor_available
              ? IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT
              : IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_PLACEHOLDER_TEXT);
    default:
      NOTREACHED();
  }
#else
  return u"Placeholder";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetNoResultsFoundDescription(QuickInsertCategory category) {
  switch (category) {
    case QuickInsertCategory::kLinks:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_BROWSING_HISTORY_LABEL_TEXT);
    case QuickInsertCategory::kClipboard:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_CLIPBOARD_LABEL_TEXT);
    case QuickInsertCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_DRIVE_FILES_LABEL_TEXT);
    case QuickInsertCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_LOCAL_FILES_LABEL_TEXT);
    case QuickInsertCategory::kDatesTimes:
    case QuickInsertCategory::kUnitsMaths:
      // TODO: b/345303965 - Add finalized strings for dates and maths.
      return l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT);
    case QuickInsertCategory::kEditorWrite:
    case QuickInsertCategory::kEditorRewrite:
    case QuickInsertCategory::kLobsterWithNoSelectedText:
    case QuickInsertCategory::kLobsterWithSelectedText:
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
      NOTREACHED();
  }
}

ui::ImageModel GetNoResultsFoundIllustration() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
      IDR_PICKER_NO_RESULTS_ILLUSTRATION);
#else
  return {};
#endif
}

bool IsEditorAvailable(
    base::span<const QuickInsertCategory> available_categories) {
  return base::Contains(available_categories,
                        QuickInsertCategory::kEditorWrite) ||
         base::Contains(available_categories,
                        QuickInsertCategory::kEditorRewrite);
}

}  // namespace

QuickInsertView::QuickInsertView(QuickInsertViewDelegate* delegate,
                                 const gfx::Rect& anchor_bounds,
                                 PickerLayoutType layout_type,
                                 PickerPositionType position_type,
                                 const base::TimeTicks trigger_event_timestamp)
    : performance_metrics_(trigger_event_timestamp), delegate_(delegate) {
  SetShowCloseButton(false);
  SetProperty(views::kElementIdentifierKey, kQuickInsertElementId);
  // TODO: b/357991165 - The desired bounds delegate here is *not* used directly
  // by the widget, because QuickInsertWidget does not use `autosize`. Rather,
  // QuickInsertView manually calls GetDesiredWidgetBounds to adjust the Widget
  // bounds to realign the search field with the caret position. Move this logic
  // to a standalone class.
  if (position_type == PickerPositionType::kNearAnchor) {
    set_desired_bounds_delegate(base::BindRepeating(
        &QuickInsertView::GetTargetBounds, base::Unretained(this),
        anchor_bounds, layout_type));
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::LayoutOrientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/kVerticalPaddingBetweenPickerContainers));

  AddMainContainerView(layout_type);
  if (base::Contains(delegate_->GetAvailableCategories(),
                     QuickInsertCategory::kEmojisGifs) ||
      base::Contains(delegate_->GetAvailableCategories(),
                     QuickInsertCategory::kEmojis)) {
    AddEmojiBarView();
  }

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));
  key_event_handler_.SetActivePseudoFocusHandler(this);

  pseudo_focused_view_tracker_.SetTrackEntireViewHierarchy(true);
  preview_bubble_observation_.Observe(&preview_controller_);
}

QuickInsertView::~QuickInsertView() = default;

bool QuickInsertView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
      if (preview_controller_.IsBubbleVisible()) {
        preview_controller_.CloseBubble();
      } else if (submenu_controller_.GetSubmenuView() != nullptr) {
        submenu_controller_.Close();
      } else if (auto* widget = GetWidget()) {
        // Otherwise, close the Quick Insert widget.
        widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      }
      return true;
    case ui::VKEY_BROWSER_BACK:
      OnSearchBackButtonPressed();
      return true;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<views::NonClientFrameView>
QuickInsertView::CreateNonClientFrameView(views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

void QuickInsertView::AddedToWidget() {
  performance_metrics_.StartRecording(*GetWidget());
  // Due to layout considerations, only populate the emoji bar after the
  // QuickInsertView has been added to a widget.
  ResetEmojiBarToZeroState();
}

void QuickInsertView::RemovedFromWidget() {
  performance_metrics_.StopRecording();
}

void QuickInsertView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  if (widget_bounds_needs_update_ && GetWidget() != nullptr) {
    GetWidget()->SetBounds(GetDesiredWidgetBounds());
    widget_bounds_needs_update_ = false;
  }
}

void QuickInsertView::SelectZeroStateCategory(QuickInsertCategory category) {
  SelectCategory(category);
}

void QuickInsertView::SelectZeroStateResult(
    const QuickInsertSearchResult& result) {
  SelectSearchResult(result);
}

QuickInsertActionType QuickInsertView::GetActionForResult(
    const QuickInsertSearchResult& result) {
  return delegate_->GetActionForResult(result);
}

void QuickInsertView::OnSearchResultsViewHeightChanged() {
  SetWidgetBoundsNeedsUpdate();
}

void QuickInsertView::GetZeroStateSuggestedResults(
    SuggestedResultsCallback callback) {
  delegate_->GetZeroStateSuggestedResults(std::move(callback));
}

void QuickInsertView::RequestPseudoFocus(views::View* view) {
  // Only allow `view` to become pseudo focused if it is visible and part of the
  // active item container.
  if (view == nullptr || !view->IsDrawn() ||
      active_item_container_ == nullptr ||
      !active_item_container_->ContainsItem(view)) {
    return;
  }
  SetPseudoFocusedView(view);
}

void QuickInsertView::OnZeroStateViewHeightChanged() {
  SetWidgetBoundsNeedsUpdate();
}

PickerCapsLockPosition QuickInsertView::GetCapsLockPosition() {
  return delegate_->GetCapsLockPosition();
}

void QuickInsertView::SetCapsLockDisplayed(bool displayed) {
  delegate_->GetSessionMetrics().SetCapsLockDisplayed(displayed);
}

void QuickInsertView::SelectSearchResult(
    const QuickInsertSearchResult& result) {
  if (const QuickInsertCategoryResult* category_data =
          std::get_if<QuickInsertCategoryResult>(&result)) {
    SelectCategory(category_data->category);
  } else if (const QuickInsertSearchRequestResult* search_request_data =
                 std::get_if<QuickInsertSearchRequestResult>(&result)) {
    UpdateSearchQueryAndActivePage(search_request_data->primary_text);
  } else if (const QuickInsertEditorResult* editor_data =
                 std::get_if<QuickInsertEditorResult>(&result)) {
    delegate_->ShowEditor(
        editor_data->preset_query_id,
        base::UTF16ToUTF8(search_field_view_->GetQueryText()));
  } else if (std::get_if<QuickInsertLobsterResult>(&result)) {
    delegate_->ShowLobster(
        base::UTF16ToUTF8(search_field_view_->GetQueryText()));
  } else {
    delegate_->GetSessionMetrics().SetSelectedResult(
        result, search_results_view_->GetIndex(result));
    switch (delegate_->GetActionForResult(result)) {
      case QuickInsertActionType::kInsert:
        delegate_->CloseWidgetThenInsertResultOnNextFocus(result);
        break;
      case QuickInsertActionType::kOpen:
      case QuickInsertActionType::kDo:
        delegate_->OpenResult(result);
        GetWidget()->Close();
        break;
      case QuickInsertActionType::kCreate:
        NOTREACHED();
    }
  }
}

void QuickInsertView::SelectMoreResults(QuickInsertSectionType type) {
  SelectCategoryWithQuery(GetCategoryForMoreResults(type),
                          search_field_view_->GetQueryText());
}

void QuickInsertView::ToggleGifs() {
  if (base::FeatureList::IsEnabled(ash::features::kPickerGifs)) {
    // TODO: b/368442959 - Search and display GIF results.
  } else {
    ShowEmojiPicker(ui::EmojiPickerCategory::kGifs);
  }
}

void QuickInsertView::ShowEmojiPicker(ui::EmojiPickerCategory category) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(QuickInsertCategory::kEmojisGifs);

  if (auto* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  session_metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kRedirected);
  delegate_->ShowEmojiPicker(category, search_field_view_->GetQueryText());
}

bool QuickInsertView::DoPseudoFocusedAction() {
  if (clear_results_timer_.IsRunning()) {
    // New results are still pending.
    // TODO: b/351920494 - Insert the first new result instead of doing nothing.
    return false;
  }

  if (auto* submenu_view = views::AsViewClass<PickerItemWithSubmenuView>(
          GetPseudoFocusedView())) {
    submenu_view->ShowSubmenu();
    SetPseudoFocusedView(submenu_controller_.GetSubmenuView()->GetTopItem());
    return true;
  }

  return GetPseudoFocusedView() == nullptr
             ? false
             : DoPickerPseudoFocusedActionOnView(GetPseudoFocusedView());
}

bool QuickInsertView::MovePseudoFocusUp() {
  if (views::View* item_above =
          active_item_container_->GetItemAbove(GetPseudoFocusedView())) {
    SetPseudoFocusedView(item_above);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kBackward);
  }
  return true;
}

bool QuickInsertView::MovePseudoFocusDown() {
  if (views::View* item_below =
          active_item_container_->GetItemBelow(GetPseudoFocusedView())) {
    SetPseudoFocusedView(item_below);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kForward);
  }
  return true;
}

bool QuickInsertView::MovePseudoFocusLeft() {
  views::View* pseudo_focused_view = GetPseudoFocusedView();
  if (IsContainedInSubmenu(pseudo_focused_view)) {
    SetPseudoFocusedView(submenu_controller_.GetAnchorView());
    submenu_controller_.Close();
    return true;
  }

  if (search_field_view_->Contains(pseudo_focused_view)) {
    if (search_field_view_->LeftEventShouldMoveCursor(pseudo_focused_view)) {
      return false;
    }
    views::View* left_view =
        search_field_view_->GetViewLeftOf(pseudo_focused_view);
    SetPseudoFocusedView(left_view);
    search_field_view_->OnGainedPseudoFocusFromLeftEvent(left_view);
    return true;
  }

  if (views::View* left_item =
          active_item_container_->GetItemLeftOf(pseudo_focused_view)) {
    SetPseudoFocusedView(left_item);
    return true;
  }
  return false;
}

bool QuickInsertView::MovePseudoFocusRight() {
  views::View* pseudo_focused_view = GetPseudoFocusedView();
  if (views::IsViewClass<PickerItemWithSubmenuView>(pseudo_focused_view)) {
    views::AsViewClass<PickerItemWithSubmenuView>(pseudo_focused_view)
        ->ShowSubmenu();
    SetPseudoFocusedView(submenu_controller_.GetSubmenuView()->GetTopItem());
    return true;
  }

  if (search_field_view_->Contains(pseudo_focused_view)) {
    if (search_field_view_->RightEventShouldMoveCursor(pseudo_focused_view)) {
      return false;
    }
    views::View* right_view =
        search_field_view_->GetViewRightOf(pseudo_focused_view);
    SetPseudoFocusedView(right_view);
    search_field_view_->OnGainedPseudoFocusFromRightEvent(right_view);
    return true;
  }

  if (views::View* right_item =
          active_item_container_->GetItemRightOf(pseudo_focused_view)) {
    SetPseudoFocusedView(right_item);
    return true;
  }
  return false;
}

bool QuickInsertView::AdvancePseudoFocus(PickerPseudoFocusDirection direction) {
  if (preview_controller_.IsBubbleVisible()) {
    preview_controller_.CloseBubble();
  }
  if (GetPseudoFocusedView() == nullptr) {
    return false;
  }
  SetPseudoFocusedView(GetNextPickerPseudoFocusableView(
      GetPseudoFocusedView(), direction, /*should_loop=*/true));
  return true;
}

void QuickInsertView::OnPreviewBubbleVisibilityChanged(bool visible) {
  if (views::Widget* widget = GetWidget()) {
    // When the bubble is visible, turn off hiding the cursor on Esc key.
    // If the cursor hides on Esc, the preview bubble is closed due to its
    // OnMouseExit event handler, before QuickInsertView has a chance to handle
    // the Esc key.
    widget->GetNativeWindow()->SetProperty(ash::kShowCursorOnKeypress, visible);
  }
}

gfx::Rect QuickInsertView::GetTargetBounds(const gfx::Rect& anchor_bounds,
                                           PickerLayoutType layout_type) {
  return delegate_->GetMode() == PickerModeType::kHasSelection
             ? GetQuickInsertViewBoundsWithSelectedText(anchor_bounds,
                                                        layout_type, size())
             : GetQuickInsertViewBoundsWithoutSelectedText(
                   anchor_bounds, layout_type, size(),
                   search_field_view_->bounds().CenterPoint().y() +
                       main_container_view_->bounds().y());
}

void QuickInsertView::UpdateSearchQueryAndActivePage(std::u16string query) {
  search_field_view_->SetQueryText(std::move(query));
  search_field_view_->RequestFocus();
  UpdateActivePage();
}

void QuickInsertView::UpdateActivePage() {
  std::u16string_view query =
      base::TrimWhitespace(search_field_view_->GetQueryText(), base::TRIM_ALL);

  if (query == last_query_ && selected_category_ == last_selected_category_) {
    return;
  }
  last_query_ = std::u16string(query);
  last_selected_category_ = selected_category_;

  delegate_->GetSessionMetrics().UpdateSearchQuery(query);

  if (!query.empty()) {
    // Don't switch the active page immediately to the search view - this will
    // be done when the clear results timer fires, or when
    // `PublishSearchResults` is called.
    clear_results_timer_.Start(
        FROM_HERE, kClearResultsTimeout,
        base::BindOnce(&QuickInsertView::OnClearResultsTimerFired,
                       weak_ptr_factory_.GetWeakPtr()));
    delegate_->StartEmojiSearch(
        query, base::BindOnce(&QuickInsertView::PublishEmojiResults,
                              weak_ptr_factory_.GetWeakPtr()));
    delegate_->StartSearch(
        query, selected_category_,
        base::BindRepeating(&QuickInsertView::PublishSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (selected_category_.has_value()) {
    SetActivePage(category_results_view_);
    if (last_suggested_results_category_ != selected_category_) {
      // Getting suggested results for a category can be slow, so show a
      // loading animation.
      category_results_view_->ShowLoadingAnimation();
      delegate_->GetResultsForCategory(
          *selected_category_,
          base::BindRepeating(&QuickInsertView::PublishCategoryResults,
                              weak_ptr_factory_.GetWeakPtr(),
                              *selected_category_));
      last_suggested_results_category_ = selected_category_;
    }
  } else {
    SetActivePage(zero_state_view_);
  }
  delegate_->StopSearch();
  clear_results_timer_.Stop();
  search_results_view_->ClearSearchResults();
  ResetEmojiBarToZeroState();
}

void QuickInsertView::PublishEmojiResults(
    std::vector<QuickInsertEmojiResult> results) {
  if (emoji_bar_view_ == nullptr) {
    return;
  }

  emoji_bar_view_->SetSearchResults(std::move(results));
  search_results_view_->SetNumEmojiResultsForA11y(
      emoji_bar_view_->GetNumItems());
}

void QuickInsertView::OnClearResultsTimerFired() {
  // `QuickInsertView::UpdateActivePage` ensures that if the active page was set
  // to the zero state or category view, the timer that this is called from is
  // cancelled - which guarantees that this can't be called.
  SetActivePage(search_results_view_);

  search_results_view_->ClearSearchResults();
  performance_metrics_.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kClear);
}

void QuickInsertView::PublishSearchResults(
    std::vector<QuickInsertSearchResultsSection> results) {
  // `QuickInsertView::UpdateActivePage` ensures that if the active page was set
  // to the zero state or category view, the delegate's search is stopped -
  // which guarantees that this can't be called.
  SetActivePage(search_results_view_);

  bool clear_stale_results = clear_results_timer_.IsRunning();
  if (clear_stale_results) {
    clear_results_timer_.Stop();
    search_results_view_->ClearSearchResults();
  }

  if (results.empty()) {
    bool no_results_found_shown = search_results_view_->SearchStopped(
        /*illustration=*/{},
        l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
    if (no_results_found_shown) {
      performance_metrics_.MarkSearchResultsUpdated(
          PickerPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);
    } else {
      CHECK(!clear_stale_results)
          << "Stale results were cleared when no results were found, but the "
             "\"no results found\" screen was not shown";
      // `clear_stale_results` must be false here, so nothing happened.
    }
    return;
  }

  for (QuickInsertSearchResultsSection& result : results) {
    search_results_view_->AppendSearchResults(std::move(result));
  }

  PickerPerformanceMetrics::SearchResultsUpdate update;
  if (clear_stale_results) {
    update = PickerPerformanceMetrics::SearchResultsUpdate::kReplace;
  } else {
    update = PickerPerformanceMetrics::SearchResultsUpdate::kAppend;
  }
  performance_metrics_.MarkSearchResultsUpdated(update);
}

void QuickInsertView::SelectCategory(QuickInsertCategory category) {
  SelectCategoryWithQuery(category, /*query=*/u"");
}

void QuickInsertView::SelectCategoryWithQuery(QuickInsertCategory category,
                                              std::u16string_view query) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(category);
  selected_category_ = category;

  if (category == QuickInsertCategory::kEmojisGifs ||
      category == QuickInsertCategory::kEmojis) {
    if (auto* widget = GetWidget()) {
      // TODO(b/316936394): Correctly handle opening of emoji picker. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open emoji picker in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis, query);
    return;
  }

  if (category == QuickInsertCategory::kEditorWrite ||
      category == QuickInsertCategory::kEditorRewrite) {
    if (auto* widget = GetWidget()) {
      // TODO: b/330267329 - Correctly handle opening of Editor. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open Editor in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    CHECK(query.empty());
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowEditor(/*preset_query_id*/ std::nullopt,
                          /*freeform_text=*/std::nullopt);
    return;
  }

  if (category == QuickInsertCategory::kLobsterWithNoSelectedText ||
      category == QuickInsertCategory::kLobsterWithSelectedText) {
    if (auto* widget = GetWidget()) {
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowLobster(/*query=*/std::nullopt);
    return;
  }

  search_field_view_->SetPlaceholderText(
      GetSearchFieldPlaceholderTextForQuickInsertCategory(category));
  search_field_view_->SetBackButtonVisible(true);
  SetEmojiBarVisibleIfEnabled(false);
  UpdateSearchQueryAndActivePage(std::u16string(query));
}

void QuickInsertView::PublishCategoryResults(
    QuickInsertCategory category,
    std::vector<QuickInsertSearchResultsSection> results) {
  category_results_view_->ClearSearchResults();

  for (QuickInsertSearchResultsSection& section : results) {
    if (!section.results().empty()) {
      category_results_view_->AppendSearchResults(std::move(section));
    }
  }

  category_results_view_->SearchStopped(GetNoResultsFoundIllustration(),
                                        GetNoResultsFoundDescription(category));
}

void QuickInsertView::AddMainContainerView(PickerLayoutType layout_type) {
  main_container_view_ =
      AddChildView(std::make_unique<PickerMainContainerView>());

  // `base::Unretained` is safe here because this class owns
  // `main_container_view_`, which owns `search_field_view_`.
  search_field_view_ = main_container_view_->AddSearchFieldView(
      views::Builder<QuickInsertSearchFieldView>(
          std::make_unique<QuickInsertSearchFieldView>(
              base::IgnoreArgs<const std::u16string&>(base::BindRepeating(
                  &QuickInsertView::UpdateActivePage, base::Unretained(this))),
              base::BindRepeating(&QuickInsertView::OnSearchBackButtonPressed,
                                  base::Unretained(this)),
              &key_event_handler_, &performance_metrics_))
          .SetPlaceholderText(GetSearchFieldPlaceholderText(
              delegate_->GetMode(),
              IsEditorAvailable(delegate_->GetAvailableCategories())))
          .Build());
  main_container_view_->AddContentsView(layout_type);

  zero_state_view_ =
      main_container_view_->AddPage(std::make_unique<PickerZeroStateView>(
          this, delegate_->GetAvailableCategories(), kQuickInsertViewWidth,
          delegate_->GetAssetFetcher(), &submenu_controller_,
          &preview_controller_));
  category_results_view_ = main_container_view_->AddPage(
      std::make_unique<QuickInsertSearchResultsView>(
          this, kQuickInsertViewWidth, delegate_->GetAssetFetcher(),
          &submenu_controller_, &preview_controller_));
  if (base::FeatureList::IsEnabled(ash::features::kPickerGrid)) {
    category_results_view_->SetLocalFileResultStyle(
        QuickInsertSearchResultsView::LocalFileResultStyle::kGrid);
  }
  search_results_view_ = main_container_view_->AddPage(
      std::make_unique<QuickInsertSearchResultsView>(
          this, kQuickInsertViewWidth, delegate_->GetAssetFetcher(),
          &submenu_controller_, &preview_controller_));

  SetActivePage(zero_state_view_);
}

void QuickInsertView::AddEmojiBarView() {
  emoji_bar_view_ =
      AddChildViewAt(std::make_unique<PickerEmojiBarView>(
                         this, kQuickInsertViewWidth,
                         /*is_gifs_enabled*/ delegate_->IsGifsEnabled()),
                     0);
}

void QuickInsertView::SetActivePage(PickerPageView* page_view) {
  main_container_view_->SetActivePage(page_view);
  SetPseudoFocusedView(nullptr);
  active_item_container_ = page_view;
  SetPseudoFocusedView(active_item_container_->GetTopItem());
  SetWidgetBoundsNeedsUpdate();
}

void QuickInsertView::SetEmojiBarVisibleIfEnabled(bool visible) {
  if (emoji_bar_view_ == nullptr) {
    return;
  }
  emoji_bar_view_->SetVisible(visible);
  SetWidgetBoundsNeedsUpdate();
}

void QuickInsertView::AdvanceActiveItemContainer(
    PickerPseudoFocusDirection direction) {
  if (active_item_container_ == submenu_controller_.GetSubmenuView()) {
    // Just keep the submenu as the active item container.
  } else if (emoji_bar_view_ == nullptr ||
             active_item_container_ == emoji_bar_view_) {
    active_item_container_ = main_container_view_;
  } else {
    active_item_container_ = emoji_bar_view_;
  }
  SetPseudoFocusedView(direction == PickerPseudoFocusDirection::kForward
                           ? active_item_container_->GetTopItem()
                           : active_item_container_->GetBottomItem());
}

void QuickInsertView::SetPseudoFocusedView(views::View* view) {
  if (view == nullptr) {
    SetPseudoFocusedView(search_field_view_->textfield());
    return;
  }

  if (pseudo_focused_view_tracker_.view() == view) {
    return;
  }

  if (IsContainedInSubmenu(view)) {
    active_item_container_ = submenu_controller_.GetSubmenuView();
  } else {
    submenu_controller_.Close();
    if (emoji_bar_view_ != nullptr && emoji_bar_view_->Contains(view)) {
      active_item_container_ = emoji_bar_view_;
    } else {
      active_item_container_ = main_container_view_;
    }
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_tracker_.view());

  pseudo_focused_view_tracker_.SetView(view);
  // base::Unretained() is safe here because this class owns
  // `pseudo_focused_view_tracker_`.
  pseudo_focused_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      &QuickInsertView::SetPseudoFocusedView, base::Unretained(this), nullptr));

  search_field_view_->SetTextfieldActiveDescendant(view);
  view->ScrollViewToVisible();
  ApplyPickerPseudoFocusToView(view);
}

views::View* QuickInsertView::GetPseudoFocusedView() {
  return pseudo_focused_view_tracker_.view();
}

void QuickInsertView::OnSearchBackButtonPressed() {
  search_field_view_->SetPlaceholderText(GetSearchFieldPlaceholderText(
      delegate_->GetMode(),
      IsEditorAvailable(delegate_->GetAvailableCategories())));
  search_field_view_->SetBackButtonVisible(false);
  SetEmojiBarVisibleIfEnabled(true);
  selected_category_ = std::nullopt;
  UpdateSearchQueryAndActivePage(u"");
  CHECK_EQ(main_container_view_->active_page(), zero_state_view_)
      << "UpdateSearchQueryAndActivePage did not set active page to zero state "
         "view";
}

void QuickInsertView::ResetEmojiBarToZeroState() {
  if (emoji_bar_view_ == nullptr) {
    return;
  }
  emoji_bar_view_->SetSearchResults(delegate_->GetSuggestedEmoji());
}

bool QuickInsertView::IsContainedInSubmenu(views::View* view) {
  return submenu_controller_.GetSubmenuView() != nullptr &&
         submenu_controller_.GetSubmenuView()->Contains(view);
}

void QuickInsertView::SetWidgetBoundsNeedsUpdate() {
  widget_bounds_needs_update_ = true;
}

BEGIN_METADATA(QuickInsertView)
END_METADATA

}  // namespace ash
