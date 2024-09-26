// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/cros_next_default_desk_button.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Base class for desk bar views, including desk bar view within overview and
// desk bar view for the desk button.
class ASH_EXPORT DeskBarViewBase : public views::View,
                                   public DesksController::Observer {
 public:
  enum class Type {
    kOverview,
    kDeskButton,
  };

  enum class State {
    kZero,
    kExpanded,
  };

  DeskBarViewBase(aura::Window* root, Type type);
  DeskBarViewBase(const DeskBarViewBase&) = delete;
  DeskBarViewBase& operator=(const DeskBarViewBase&) = delete;
  ~DeskBarViewBase() override;

  // Returns the preferred height of the desk bar that exists on `root` with
  // `state`.
  static int GetPreferredBarHeight(aura::Window* root, Type type, State state);

  // Returns the preferred state for the desk bar given `type`.
  static State GetPerferredState(Type type);

  // Creates and returns the widget that contains the desk bar view of `type`.
  // The returned widget has no contents view yet, and hasn't been shown yet.
  static std::unique_ptr<views::Widget>
  CreateDeskWidget(aura::Window* root, const gfx::Rect& bounds, Type type);

  Type type() const { return type_; }

  State state() const { return state_; }

  void set_is_bounds_animation_on_going(bool value) {
    is_bounds_animation_on_going_ = value;
  }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  OverviewGrid* overview_grid() const { return overview_grid_; }
  void set_overview_grid(OverviewGrid* overview_grid) {
    overview_grid_ = overview_grid;
  }

  const std::vector<DeskMiniView*>& mini_views() const { return mini_views_; }

  const views::View* scroll_view_contents() const {
    return scroll_view_contents_;
  }

  ZeroStateDefaultDeskButton* zero_state_default_desk_button() const {
    return zero_state_default_desk_button_;
  }

  ZeroStateIconButton* zero_state_new_desk_button() const {
    return zero_state_new_desk_button_;
  }

  ExpandedDesksBarButton* expanded_state_new_desk_button() const {
    return expanded_state_new_desk_button_;
  }

  ZeroStateIconButton* zero_state_library_button() const {
    return zero_state_library_button_;
  }

  ExpandedDesksBarButton* expanded_state_library_button() const {
    return expanded_state_library_button_;
  }

  CrOSNextDefaultDeskButton* default_desk_button() {
    return default_desk_button_;
  }
  const CrOSNextDefaultDeskButton* default_desk_button() const {
    return default_desk_button_;
  }

  CrOSNextDeskIconButton* new_desk_button() { return new_desk_button_; }
  const CrOSNextDeskIconButton* new_desk_button() const {
    return new_desk_button_;
  }

  CrOSNextDeskIconButton* library_button() { return library_button_; }
  const CrOSNextDeskIconButton* library_button() const {
    return library_button_;
  }

  views::Label* new_desk_button_label() { return new_desk_button_label_; }
  const views::Label* new_desk_button_label() const {
    return new_desk_button_label_;
  }

  views::Label* library_button_label() { return library_button_label_; }
  const views::Label* library_button_label() const {
    return library_button_label_;
  }

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Initializes and creates mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  virtual void Init();

  // Returns true if it is currently in zero state.
  bool IsZeroState() const;

  // If a desk is in a drag & drop cycle.
  bool IsDraggingDesk() const;

  // Returns true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

  // If the focused `view` is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen. Please note, `view`
  // must be a child of `scroll_view_contents_`.
  void ScrollToShowViewIfNecessary(const views::View* view);

  // Returns the mini_view associated with `desk` or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Get the index of a desk mini view in the `mini_views`.
  int GetMiniViewIndex(const DeskMiniView* mini_view) const;

  void OnNewDeskButtonPressed(
      DesksCreationRemovalSource desks_creation_removal_source);

  // Called when the saved desk library is hidden. Transitions the desk bar
  // view to zero state if necessary.
  void OnSavedDeskLibraryHidden();

  // Bring focus to the name view of the desk with `desk_index`.
  void NudgeDeskName(int desk_index);

  // If in expanded state, updates the border color of the
  // `expanded_state_library_button_` and the active desk's mini view
  // after the saved desk library has been shown. If not in expanded state,
  // updates the background color of the `zero_state_library_button_`
  // and the `zero_state_default_desk_button_`.
  void UpdateButtonsForSavedDeskGrid();

  // Updates the visibility of the two buttons inside the zero state desk bar
  // and the `ExpandedDesksBarButton` on the desk bar's state.
  void UpdateDeskButtonsVisibility();

  // Udates the visibility of the `default_desk_button_` on the desk bar's
  // state.
  // TODO(conniekxu): Remove `UpdateDeskButtonsVisibility`, replace it with this
  // function, and rename this function by removing the prefix CrOSNext.
  void UpdateDeskButtonsVisibilityCrOSNext();

  // Updates the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled, the user has any saved desks and the
  // state of the desk bar.
  void UpdateLibraryButtonVisibility();

  // Updates the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled and the user has any saved desks.
  // TODO(conniekxu): Remove `UpdateLibraryButtonVisibility`, replace it with
  // this function, and rename this function by removing the prefix CrOSNext.
  void UpdateLibraryButtonVisibilityCrOSNext();

  // Called to update state of `button` and apply the scale animation to the
  // button. For the new desk button, this is called when the make the new desk
  // button a drop target for the window being dragged or at the end of the
  // drag. For the library button, this is called when the library is clicked at
  // the expanded state. Please note this will only be used to switch the states
  // of the `button` between the expanded and active.
  virtual void UpdateDeskIconButtonState(
      CrOSNextDeskIconButton* button,
      CrOSNextDeskIconButton::State target_state);

  virtual void UpdateNewMiniViews(bool initializing_bar_view,
                                  bool expanding_bar_view);

  virtual void SwitchToZeroState();

  virtual void SwitchToExpandedState();

  virtual void HandlePressEvent(DeskMiniView* mini_view,
                                const ui::LocatedEvent& event);

  virtual void HandleLongPressEvent(DeskMiniView* mini_view,
                                    const ui::LocatedEvent& event);

  virtual void HandleDragEvent(DeskMiniView* mini_view,
                               const ui::LocatedEvent& event);

  virtual bool HandleReleaseEvent(DeskMiniView* mini_view,
                                  const ui::LocatedEvent& event);

 protected:
  // Returns the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  // Determine the new index of the dragged desk at the position of
  // `location_in_screen`.
  int DetermineMoveIndex(int location_in_screen) const;

  // Updates the visibility of `left_scroll_button_` and `right_scroll_button_`.
  // Show `left_scroll_button_` if there are contents outside of the left edge
  // of the `scroll_view_`, the same for `right_scroll_button_` based on the
  // right side of the `scroll_view_`.
  void UpdateScrollButtonsVisibility();

  // We will show a fade in gradient besides `left_scroll_button_` and a fade
  // out gradient besides `right_scroll_button_`. Show the gradient only when
  // the corresponding scroll button is visible.
  void UpdateGradientMask();

  // Scrolls the desk bar to the previous or next page. The page size is the
  // width of the scroll view, the contents that are outside of the scroll view
  // will be clipped and can not be seen.
  void ScrollToPreviousPage();
  void ScrollToNextPage();

  // Gets the adjusted scroll position based on `position` to make sure no desk
  // preview is cropped at the start position of the scrollable bar.
  int GetAdjustedUncroppedScrollPosition(int position) const;

  void OnLibraryButtonPressed();

  // This function cycles through `mini_views_` and updates the tooltip for each
  // mini view's combine desks button.
  void MaybeUpdateCombineDesksTooltips();

  // Scrollview callbacks.
  void OnContentsScrolled();
  void OnContentsScrollEnded();

  const Type type_ = Type::kOverview;

  State state_ = State::kZero;

  // True if the `DesksBarBoundsAnimation` is started and hasn't finished yet.
  // It will be used to hold `Layout` until the bounds animation is completed.
  // `Layout` is expensive and will be called on bounds changes, which means it
  // will be called lots of times during the bounds changes animation. This is
  // done to eliminate the unnecessary `Layout` calls during the animation.
  bool is_bounds_animation_on_going_ = false;

  // Mini view whose preview is being dragged.
  DeskMiniView* drag_view_ = nullptr;

  // The screen location of the most recent drag position. This value is valid
  // only when the below `dragged_item_over_bar_` is true.
  gfx::Point last_dragged_item_screen_location_;

  // True when the drag location of the overview item is intersecting with this
  // view.
  bool dragged_item_over_bar_ = false;

  // The `OverviewGrid` that contains this object if this is a `Type::kOverview`
  // bar, nullptr otherwise.
  OverviewGrid* overview_grid_;

  // The views representing desks mini_views. They're owned by views hierarchy.
  std::vector<DeskMiniView*> mini_views_;

  // Puts the contents in a `ScrollView` to support scrollable desks.
  views::ScrollView* scroll_view_ = nullptr;

  // Contents of `scroll_view_`, which includes `mini_views_`,
  // `expanded_state_new_desk_button_` and optionally
  // `expanded_state_library_button_` currently.
  views::View* scroll_view_contents_ = nullptr;

  // Default desk button and new desk buttons.
  raw_ptr<ZeroStateDefaultDeskButton, ExperimentalAsh>
      zero_state_default_desk_button_ = nullptr;
  raw_ptr<ZeroStateIconButton, ExperimentalAsh> zero_state_new_desk_button_ =
      nullptr;
  raw_ptr<ExpandedDesksBarButton, ExperimentalAsh>
      expanded_state_new_desk_button_ = nullptr;

  // Buttons to show the saved desk grid.
  raw_ptr<ZeroStateIconButton, ExperimentalAsh> zero_state_library_button_ =
      nullptr;
  raw_ptr<ExpandedDesksBarButton, ExperimentalAsh>
      expanded_state_library_button_ = nullptr;

  // Buttons for the CrOS Next updated UI. They're added behind the feature flag
  // Jellyroll.
  // TODO(conniekxu): After CrOS Next is launched, replace
  // `zero_state_default_desk_button_`, `zero_state_default_desk_button_`,
  // `expanded_state_new_desk_button_`, `zero_state_library_button_` and
  // `expanded_state_library_button_` with the buttons below.
  raw_ptr<CrOSNextDefaultDeskButton, ExperimentalAsh> default_desk_button_ =
      nullptr;
  raw_ptr<CrOSNextDeskIconButton, ExperimentalAsh> new_desk_button_ = nullptr;
  raw_ptr<CrOSNextDeskIconButton, ExperimentalAsh> library_button_ = nullptr;

  // Labels to be shown under the desk icon buttons when they're at the active
  // state.
  raw_ptr<views::Label, ExperimentalAsh> new_desk_button_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> library_button_label_ = nullptr;

  // Scroll arrow buttons.
  raw_ptr<ScrollArrowButton, ExperimentalAsh> left_scroll_button_ = nullptr;
  raw_ptr<ScrollArrowButton, ExperimentalAsh> right_scroll_button_ = nullptr;

  // ScrollView callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;

  raw_ptr<aura::Window> root_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
