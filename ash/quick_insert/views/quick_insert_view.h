// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/quick_insert/metrics/quick_insert_performance_metrics.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_key_event_handler.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus_handler.h"
#include "ash/quick_insert/views/quick_insert_search_results_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/quick_insert/views/quick_insert_zero_state_view_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

enum class QuickInsertCapsLockPosition;
enum class QuickInsertLayoutType;
enum class QuickInsertPositionType;
enum class QuickInsertPseudoFocusDirection;
class QuickInsertEmojiBarView;
class QuickInsertMainContainerView;
class QuickInsertSearchFieldView;
class QuickInsertPageView;
class QuickInsertSearchResultsSection;
class QuickInsertSearchResultsView;
class QuickInsertTraversableItemContainer;
class QuickInsertViewDelegate;
class QuickInsertZeroStateView;

// View for the Quick Insert widget.
class ASH_EXPORT QuickInsertView
    : public views::WidgetDelegateView,
      public QuickInsertZeroStateViewDelegate,
      public QuickInsertSearchResultsViewDelegate,
      public QuickInsertEmojiBarViewDelegate,
      public QuickInsertPseudoFocusHandler,
      public QuickInsertPreviewBubbleController::Observer {
  METADATA_HEADER(QuickInsertView, views::WidgetDelegateView)

 public:
  // `delegate` must remain valid for the lifetime of this class.
  explicit QuickInsertView(QuickInsertViewDelegate* delegate,
                           const gfx::Rect& anchor_bounds,
                           QuickInsertLayoutType layout_type,
                           QuickInsertPositionType position_type,
                           base::TimeTicks trigger_event_timestamp);
  QuickInsertView(const QuickInsertView&) = delete;
  QuickInsertView& operator=(const QuickInsertView&) = delete;
  ~QuickInsertView() override;

  // Time from when a search starts to when the previous set of results are
  // cleared.
  // Slightly longer than the real burn in period to ensure empty results do not
  // flash on the screen before showing burn-in results.
  static constexpr base::TimeDelta kClearResultsTimeout =
      QuickInsertController::kBurnInPeriod + base::Milliseconds(50);

  // views::WidgetDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;

  // QuickInsertZeroStateViewDelegate:
  void SelectZeroStateCategory(QuickInsertCategory category) override;
  void SelectZeroStateResult(const QuickInsertSearchResult& result) override;
  void GetZeroStateSuggestedResults(SuggestedResultsCallback callback) override;
  void RequestPseudoFocus(views::View* view) override;
  void OnZeroStateViewHeightChanged() override;
  void SetCapsLockDisplayed(bool displayed) override;
  QuickInsertCapsLockPosition GetCapsLockPosition() override;

  // QuickInsertSearchResultsViewDelegate:
  void SelectSearchResult(const QuickInsertSearchResult& result) override;
  void SelectMoreResults(QuickInsertSectionType type) override;
  QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) override;
  void OnSearchResultsViewHeightChanged() override;

  // QuickInsertEmojiBarViewDelegate:
  void ToggleGifs(bool is_checked) override;
  void ShowEmojiPicker(ui::EmojiPickerCategory category) override;

  // QuickInsertPseudoFocusHandler:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  bool AdvancePseudoFocus(QuickInsertPseudoFocusDirection direction) override;

  // QuickInsertPreviewBubbleController::Observer:
  void OnPreviewBubbleVisibilityChanged(bool visible) override;

  // Returns the target bounds for this Quick Insert view. The target bounds try
  // to vertically align `search_field_view_` with `anchor_bounds`.
  // `anchor_bounds` and returned bounds should be in screen coordinates.
  gfx::Rect GetTargetBounds(const gfx::Rect& anchor_bounds,
                            QuickInsertLayoutType layout_type);

  QuickInsertSubmenuController& submenu_controller_for_testing() {
    return submenu_controller_;
  }
  QuickInsertPreviewBubbleController& preview_controller_for_testing() {
    return preview_controller_;
  }

  QuickInsertSearchFieldView& search_field_view_for_testing() {
    return *search_field_view_;
  }
  QuickInsertSearchResultsView& search_results_view_for_testing() {
    return *search_results_view_;
  }
  QuickInsertSearchResultsView& category_results_view_for_testing() {
    return *category_results_view_;
  }
  QuickInsertZeroStateView& zero_state_view_for_testing() {
    return *zero_state_view_;
  }
  QuickInsertEmojiBarView* emoji_bar_view_for_testing() {
    return emoji_bar_view_;
  }

 private:
  // Sets the search text field's query text to the query, focuses it, then
  // updates the active page - starting / ending a search if necessary.
  void UpdateSearchQueryAndActivePage(std::u16string query);

  // Updates the active page based on the search text field's query text, as
  // well as the active category.
  // If the search text field's query text is non-empty, this starts a search
  // and sets the active page to the search view after a delay via
  // `OnClearResultsTimerFired` and `PublishSearchResults`.
  // Otherwise, stops any previous searches and immediately sets the active page
  // to the zero state / category results view, fetching category results if
  // necessary.
  void UpdateActivePage();

  // Displays `results` in the emoji bar.
  void PublishEmojiResults(std::vector<QuickInsertEmojiResult> results);

  // Clears the search results and sets the active page to the search view.
  void OnClearResultsTimerFired();

  // Displays `results` in the search view and sets it as the active page.
  // If `results` is empty and no results were previously published, then a "no
  // results found" view is shown instead of a blank view.
  void PublishSearchResults(
      std::vector<QuickInsertSearchResultsSection> results);

  // Selects a category. This shows the category view and fetches zero-state
  // results for the category, which are returned to `PublishCategoryResults`.
  void SelectCategory(QuickInsertCategory category);

  // Selects a category. This shows the category view and fetches search
  // results for the category based on `query`, which are returned to
  // `PublishSearchResults`.
  void SelectCategoryWithQuery(QuickInsertCategory category,
                               std::u16string_view query);

  // Displays `results` in the category view.
  void PublishCategoryResults(
      QuickInsertCategory category,
      std::vector<QuickInsertSearchResultsSection> results);

  // Adds the main container, which includes the search field and contents
  // pages.
  void AddMainContainerView(QuickInsertLayoutType layout_type);

  // Adds the emoji bar, which contains emoji and other expression results and
  // is shown above the main container.
  void AddEmojiBarView();

  // Sets `page_view` as the active page in `main_container_view_`.
  void SetActivePage(QuickInsertPageView* page_view);

  // Sets emoji bar visibility, or does nothing if the emoji bar is not enabled.
  void SetEmojiBarVisibleIfEnabled(bool visible);

  // Moves pseudo focus between different parts of the QuickInsertView, i.e.
  // between the emoji bar and the main container.
  void AdvanceActiveItemContainer(QuickInsertPseudoFocusDirection direction);

  // Sets `view` as the pseudo focused view, i.e. the view which responds to
  // user actions that trigger `DoPseudoFocusedAction`. If `view` is null,
  // pseudo focus instead moves back to the search field.
  void SetPseudoFocusedView(views::View* view);

  views::View* GetPseudoFocusedView();

  // Removes the currently selected category filter, with the option to clear
  // the search field.
  void ResetSelectedCategory(bool reset_query);

  // Called when the search field back button is pressed.
  void OnSearchBackButtonPressed();

  // Clears the current results in the emoji bar and shows recent and
  // placeholder emojis instead.
  void ResetEmojiBarToZeroState();

  // Returns true if `view` is contained in a submenu of this QuickInsertView.
  bool IsContainedInSubmenu(views::View* view);

  // Called to indicate that the Quick Insert widget bounds need to be be
  // updated (e.g. to re-align the Quick Insert search field after results have
  // changed).
  void SetWidgetBoundsNeedsUpdate();

  // The currently selected category.
  // Should only be set to `std::nullopt` through `OnSearchBackButtonPressed`.
  // Should only be set to a value through `SelectCategory` and
  // `SelectCategoryWithQuery`.
  std::optional<QuickInsertCategory> selected_category_;

  // Whether the GIF toggle is checked (i.e. should only show GIF results).
  bool is_gif_toggle_checked_ = false;

  // The category which `category_results_view_` has results for.
  // Used for caching results if the user did not change their selected
  // category.
  // For example:
  // - When a user starts a filtered search from a category's suggested results,
  //   then clears the search query, the old suggested results are not cleared
  //   as `last_suggested_results_category_ == selected_category_`.
  // - When a user starts a non-filtered search from zero state, then filters
  //   results to a category, then clears the search query, new results will be
  //   fetched as the `last_suggested_results_category_ != selected_category_`.
  std::optional<QuickInsertCategory> last_suggested_results_category_;
  // The whitespace-trimmed query and category when `UpdateActivePage()` was
  // last called.
  // Used for avoid unnecessary searches if `UpdateActivePage()` is called again
  // with the same {query, selected_category, is_gif_toggle_checked}.
  std::u16string last_query_;
  std::optional<QuickInsertCategory> last_selected_category_;
  bool last_is_gif_toggle_checked_ = false;

  QuickInsertKeyEventHandler key_event_handler_;
  QuickInsertSubmenuController submenu_controller_;
  QuickInsertPreviewBubbleController preview_controller_;
  QuickInsertPerformanceMetrics performance_metrics_;
  raw_ptr<QuickInsertViewDelegate> delegate_ = nullptr;

  // The main container contains the search field and contents pages.
  raw_ptr<QuickInsertMainContainerView> main_container_view_ = nullptr;
  raw_ptr<QuickInsertSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<QuickInsertZeroStateView> zero_state_view_ = nullptr;
  raw_ptr<QuickInsertSearchResultsView> category_results_view_ = nullptr;
  raw_ptr<QuickInsertSearchResultsView> search_results_view_ = nullptr;

  raw_ptr<QuickInsertEmojiBarView> emoji_bar_view_ = nullptr;

  // The item container which contains `pseudo_focused_view_` and will respond
  // to keyboard navigation events.
  raw_ptr<QuickInsertTraversableItemContainer> active_item_container_ = nullptr;

  // Tracks the currently pseudo focused view, which responds to user actions
  // that trigger `DoPseudoFocusedAction`.
  views::ViewTracker pseudo_focused_view_tracker_;

  // If true, the Widget bounds should be adjusted on the next layout.
  bool widget_bounds_needs_update_ = true;

  // Clears `search_results_view_`'s old search results when a new search is
  // started - after `kClearResultsTimeout`, or when the first search results
  // come in (whatever is earliest).
  // This timer is running iff the first set of results for the current search
  // have not been published yet.
  base::OneShotTimer clear_results_timer_;

  base::ScopedObservation<QuickInsertPreviewBubbleController,
                          QuickInsertPreviewBubbleController::Observer>
      preview_bubble_observation_{this};

  base::WeakPtrFactory<QuickInsertView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_VIEW_H_
