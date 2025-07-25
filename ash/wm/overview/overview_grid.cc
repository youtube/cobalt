// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/cros_next_default_desk_button.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_animations.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_focus_cycler.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/adapters.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Values for the no items indicator which appears when opening overview mode
// with no opened windows.
constexpr int kNoItemsIndicatorHeightDp = 32;
constexpr int kNoItemsIndicatorHorizontalPaddingDp = 16;
constexpr int kNoItemsIndicatorRoundingDp = 16;
constexpr int kNoItemsIndicatorVerticalPaddingDp = 8;

// Distance from the bottom of the save desk as template button to the top of
// the first overview item.
constexpr int kSaveDeskAsTemplateOverviewItemSpacingDp = 45;

// Distance from the bottom of the last overview item to the top of the faster
// splitscreen toast widget.
constexpr int kFasterSplitScreenToastSpacingDp = 40;

// Windows are not allowed to get taller than this.
constexpr int kMaxHeight = 512;

// Margins reserved in the overview mode.
constexpr float kOverviewInsetRatio = 0.05f;

// Additional vertical inset reserved for windows in overview mode.
constexpr float kOverviewVerticalInset = 0.1f;

// Number of rows for windows in tablet overview mode.
constexpr int kScrollingLayoutRow = 2;

constexpr int kMinimumItemsForScrollingLayout = 6;

constexpr int kTabletModeOverviewItemTopPaddingDp = 16;

// The threshold for expanding desks bar while dragging the window. When the
// length between the center point of the window being dragged and the center
// point of the `zero_state_new_desk_button_` is smaller than
// `kExpandDesksBarThreshold`, desks bar will be transformed from zero state to
// expanded state to help user dropping the dragged window on the new desk.
constexpr int kExpandDesksBarThreshold = 130;

// Wait a while before unpausing the occlusion tracker after a scroll has
// completed as the user may start another scroll.
constexpr base::TimeDelta kOcclusionUnpauseDurationForScroll =
    base::Milliseconds(500);

constexpr base::TimeDelta kOcclusionUnpauseDurationForRotation =
    base::Milliseconds(300);

// TODO(b/305980930): Replace with UX string when it is approved.
const std::u16string kFasterSplitScreenToastNoWindows =
    u"Create a window to use split screen, or click empty area to skip.";
const std::u16string kFasterSplitScreenToast =
    u"Choose a window to use split screen, or click empty area to skip.";

// Toast id for the toast that is displayed when a user tries to move a window
// that is visible on all desks to another desk.
constexpr char kMoveVisibleOnAllDesksWindowToastId[] =
    "ash.wm.overview.move_visible_on_all_desks_window_toast";

// Histogram names for overview enter/exit smoothness in clamshell,
// tablet mode and splitview.
constexpr char kOverviewEnterClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.ClamshellMode";
constexpr char kOverviewEnterSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SingleClamshellMode";
constexpr char kOverviewEnterTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.TabletMode";
constexpr char kOverviewEnterMinimizedTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.MinimizedTabletMode";
constexpr char kOverviewEnterSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SplitView";

constexpr char kOverviewExitClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.ClamshellMode";
constexpr char kOverviewExitSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SingleClamshellMode";
constexpr char kOverviewExitTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.TabletMode";
constexpr char kOverviewExitMinimizedTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.MinimizedTabletMode";
constexpr char kOverviewExitSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SplitView";

// The UMA histogram that records presentation time for grid scrolling in the
// new overview layout.
constexpr char kOverviewScrollHistogram[] =
    "Ash.Overview.Scroll.PresentationTime.TabletMode";
constexpr char kOverviewScrollMaxLatencyHistogram[] =
    "Ash.Overview.Scroll.PresentationTime.MaxLatency.TabletMode";

template <const char* clamshell_single_name,
          const char* clamshell_multi_name,
          const char* tablet_name,
          const char* splitview_name,
          const char* tablet_minimized_name>
class OverviewMetricsTracker : public OverviewGrid::MetricsTracker {
 public:
  OverviewMetricsTracker(ui::Compositor* compositor,
                         bool in_split_view,
                         bool single_animation_in_clamshell,
                         bool minimized_in_tablet)
      : tracker_(compositor->RequestNewThroughputTracker()) {
    tracker_.Start(metrics_util::ForSmoothnessV3(base::BindRepeating(
        &OverviewMetricsTracker::ReportOverviewSmoothness, in_split_view,
        single_animation_in_clamshell, minimized_in_tablet)));
  }
  OverviewMetricsTracker(const OverviewMetricsTracker&) = delete;
  OverviewMetricsTracker& operator=(const OverviewMetricsTracker&) = delete;
  ~OverviewMetricsTracker() override { tracker_.Stop(); }

  static void ReportOverviewSmoothness(bool in_split_view,
                                       bool single_animation_in_clamshell,
                                       bool minimized_in_tablet,
                                       int smoothness) {
    if (single_animation_in_clamshell)
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_single_name, smoothness);
    else
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_multi_name, smoothness);

    if (minimized_in_tablet) {
      UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(
          in_split_view, tablet_minimized_name, smoothness);
    } else {
      UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(in_split_view,
                                                       tablet_name, smoothness);
    }
    UMA_HISTOGRAM_PERCENTAGE_IN_SPLITVIEW(in_split_view, splitview_name,
                                          smoothness);
  }

 private:
  ui::ThroughputTracker tracker_;
};

using OverviewEnterMetricsTracker =
    OverviewMetricsTracker<kOverviewEnterSingleClamshellHistogram,
                           kOverviewEnterClamshellHistogram,
                           kOverviewEnterTabletHistogram,
                           kOverviewEnterSplitViewHistogram,
                           kOverviewEnterMinimizedTabletHistogram>;
using OverviewExitMetricsTracker =
    OverviewMetricsTracker<kOverviewExitSingleClamshellHistogram,
                           kOverviewExitClamshellHistogram,
                           kOverviewExitTabletHistogram,
                           kOverviewExitSplitViewHistogram,
                           kOverviewExitMinimizedTabletHistogram>;

class ShutdownAnimationMetricsTrackerObserver : public OverviewObserver,
                                                public ui::CompositorObserver {
 public:
  ShutdownAnimationMetricsTrackerObserver(ui::Compositor* compositor,
                                          bool in_split_view,
                                          bool single_animation,
                                          bool minimized_in_tablet)
      : compositor_(compositor),
        metrics_tracker_(compositor,
                         in_split_view,
                         single_animation,
                         minimized_in_tablet) {
    compositor->AddObserver(this);
    OverviewController::Get()->AddObserver(this);
  }
  ShutdownAnimationMetricsTrackerObserver(
      const ShutdownAnimationMetricsTrackerObserver&) = delete;
  ShutdownAnimationMetricsTrackerObserver& operator=(
      const ShutdownAnimationMetricsTrackerObserver&) = delete;
  ~ShutdownAnimationMetricsTrackerObserver() override {
    compositor_->RemoveObserver(this);
    if (OverviewController* overview_controller =
            Shell::Get()->overview_controller()) {
      overview_controller->RemoveObserver(this);
    }
  }

  // OverviewObserver:
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    delete this;
  }

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    delete this;
  }

 private:
  raw_ptr<ui::Compositor, ExperimentalAsh> compositor_;
  OverviewExitMetricsTracker metrics_tracker_;
};

// Creates `save_desk_button_container_widget_`. It contains SaveDeskAsTemplate
// button and save for later button.
std::unique_ptr<views::Widget> CreateSaveDeskButtonContainerWidget(
    aura::Window* root_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  // If Chromevox is on, let the widget be activatable.
  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
  if (spoken_feedback_enabled)
    params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "SaveDeskButtonContainerWidget";
  params.accept_events = true;
  // This widget is hidden during window dragging, but will become visible on
  // mouse/touch release. Place it in the active desk container so it remains
  // beneath the dragged window when it is animating back to the overview grid.
  params.parent = desks_util::GetActiveDeskContainerForRoot(root_window);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  // This should not show up in the MRU list. Otherwise, it will be treated as
  // unsupported crostini app.
  params.init_properties_container.SetProperty(kOverviewUiKey, true);

  auto widget = std::make_unique<views::Widget>();
  widget->set_focus_on_creation(false);
  widget->Init(std::move(params));
  // Turn off default widget animations.
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  aura::Window* window = widget->GetNativeWindow();
  window->parent()->StackChildAtBottom(window);
  return widget;
}

float GetWantedDropTargetOpacity(
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  switch (window_dragging_state) {
    case SplitViewDragIndicators::WindowDraggingState::kNoDrag:
    case SplitViewDragIndicators::WindowDraggingState::kOtherDisplay:
    case SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary:
    case SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary:
      return 0.f;
    case SplitViewDragIndicators::WindowDraggingState::kFromOverview:
    case SplitViewDragIndicators::WindowDraggingState::kFromTop:
    case SplitViewDragIndicators::WindowDraggingState::kFromShelf:
      return 1.f;
    case SplitViewDragIndicators::WindowDraggingState::kFromFloat:
      NOTREACHED();
      return 0.f;
  }
}

gfx::Insets GetGridInsetsImpl(const gfx::Rect& grid_bounds) {
  const int horizontal_inset =
      base::ClampFloor(kOverviewInsetRatio *
                       std::min(grid_bounds.width(), grid_bounds.height()));
  const int vertical_inset =
      horizontal_inset +
      kOverviewVerticalInset * (grid_bounds.height() - 2 * horizontal_inset);

  return gfx::Insets::VH(std::max(0, vertical_inset),
                         std::max(0, horizontal_inset));
}

bool ShouldExcludeItemFromGridLayout(
    OverviewItemBase* item,
    const base::flat_set<OverviewItemBase*>& ignored_items) {
  return item->animating_to_close() || ignored_items.contains(item);
}

bool IsUnsupportedWindow(aura::Window* window) {
  const bool has_restore_id =
      !wm::GetTransientParent(window) &&
      (OverviewController::Get()->disable_app_id_check_for_saved_desks() ||
       !saved_desk_util::GetAppId(window).empty());

  return !DeskTemplate::IsAppTypeSupported(window) || !has_restore_id;
}

bool IsIncognitoWindow(aura::Window* window) {
  return !Shell::Get()->saved_desk_delegate()->IsWindowPersistable(window);
}

// Returns the window(s) associated with dragging which can be the window(s)
// represented by the `OverviewItemBase` or a preset `dragged_window`.
aura::Window::Windows GetWindowsAssociatedWithDragging(
    OverviewItemBase* grid_dragged_item,
    aura::Window* dragged_window) {
  return grid_dragged_item ? grid_dragged_item->GetWindows()
                           : aura::Window::Windows({dragged_window});
}

// Returns true if all the `windows` associated with the drag are not null and
// have parent.
bool AreDraggedWindowsValid(const aura::Window::Windows& windows) {
  for (const auto* window : windows) {
    if (!window || !window->parent()) {
      return false;
    }
  }

  return true;
}

// Returns true if all the `windows` associated with the drag are maximized.
bool AreAllWindowsMaximized(const aura::Window::Windows& windows) {
  for (const auto* window : windows) {
    if (!WindowState::Get(window)->IsMaximized()) {
      return false;
    }
  }

  return true;
}

// Returns the total size of the `windows` associated with the drag.
gfx::Size GetTotalDraggedWindowsSize(const aura::Window::Windows& windows) {
  gfx::Rect total_bounds;
  for (auto* win : windows) {
    total_bounds.Union(win->bounds());
  }

  return total_bounds.size();
}

// Returns the total size of the given `windows` including the transient
// children. If the given `windows` belong to the same snap group, the total
// size needs to be enlarged to include the size of the divider.
gfx::SizeF GetTotalUnionSizeIncludingTransients(
    const aura::Window::Windows& windows) {
  gfx::RectF total_bounds;
  for (auto* win : windows) {
    total_bounds.Union(GetUnionScreenBoundsForWindow(win));
  }

  gfx::SizeF total_size = total_bounds.size();
  // TODO(michelefan): Add extra width of the divider for the height of the
  // `total_size` in portrait mode.
  if (windows.size() == 2u) {
    total_size.Enlarge(kSplitviewDividerShortSideLength, 0);
  }

  return total_size;
}

// Returns the maximum of the `aura::client::kTopViewInset` among the `windows`.
int GetTopViewInset(const aura::Window::Windows& windows) {
  int inset = 0;
  for (auto* win : windows) {
    const int win_inset = win->GetProperty(aura::client::kTopViewInset);
    inset = std::max(inset, win_inset);
  }

  return inset;
}

// Returns the corresponding `SplitViewOverviewSessionExitPoint` with the
// overview end action deduced from the given `overview_session`.
SplitViewOverviewSessionExitPoint GetSplitViewOverviewSessionExitPoint(
    OverviewSession* overview_session) {
  OverviewEndAction overview_end_action =
      overview_session->overview_end_action();
  if (overview_end_action == OverviewEndAction::kWindowActivating) {
    return SplitViewOverviewSessionExitPoint::kCompleteByActivating;
  } else if (overview_end_action == OverviewEndAction::kKeyEscapeOrBack ||
             overview_end_action ==
                 OverviewEndAction::kClickingOutsideWindowsInOverview) {
    return SplitViewOverviewSessionExitPoint::kSkip;
  } else if (overview_end_action == OverviewEndAction::kShuttingDown) {
    return SplitViewOverviewSessionExitPoint::kShutdown;
  }
  return SplitViewOverviewSessionExitPoint::kUnspecified;
}

// Returns true if any of the items in `grid` covers the entire workspace, false
// otherwise.
bool ShouldImmediatelyInitDeskBar(OverviewGrid* grid) {
  // Always immediately initialize the desk bar when doing app dragging or when
  // immediately exiting.
  const OverviewEnterExitType enter_exit_type =
      grid->overview_session()->enter_exit_overview_type();
  if (enter_exit_type == OverviewEnterExitType::kImmediateEnter ||
      enter_exit_type == OverviewEnterExitType::kImmediateEnterWithoutFocus ||
      enter_exit_type == OverviewEnterExitType::kImmediateExit) {
    return true;
  }

  // If one of the windows covers the workspace, we can create the desks bar
  // immediately.
  for (const auto& overview_item : grid->window_list()) {
    if (CanCoverAvailableWorkspace(overview_item->GetWindow())) {
      return true;
    }
  }

  return false;
}

}  // namespace

OverviewGrid::OverviewGrid(aura::Window* root_window,
                           const std::vector<aura::Window*>& windows,
                           OverviewSession* overview_session)
    : root_window_(root_window),
      overview_session_(overview_session),
      split_view_drag_indicators_(
          ShouldAllowSplitView()
              ? std::make_unique<SplitViewDragIndicators>(root_window)
              : nullptr),
      bounds_(GetGridBoundsInScreen(root_window)) {
  TRACE_EVENT0("ui", "OverviewGrid::OverviewGrid");

  for (auto* window : windows) {
    if (window->GetRootWindow() != root_window)
      continue;

    // Stop ongoing animations before entering overview mode. Because we are
    // deferring SetTransform of the windows beneath the window covering the
    // available workspace, we need to set the correct transforms of these
    // windows before entering overview mode again in the
    // OnImplicitAnimationsCompleted() of the observer of the
    // available-workspace-covering window's animation.
    if (auto* animator = window->layer()->GetAnimator();
        animator && animator->is_animating()) {
      animator->StopAnimating();
    }

    std::unique_ptr<OverviewItemBase> overview_item_base =
        OverviewItemBase::Create(window, overview_session_, this);
    UpdateNumSavedDeskUnsupportedWindows(overview_item_base->GetWindows(),
                                         /*increment=*/true);
    window_list_.push_back(std::move(overview_item_base));
  }
}

OverviewGrid::~OverviewGrid() = default;

void OverviewGrid::Shutdown(OverviewEnterExitType exit_type) {
  TRACE_EVENT0("ui", "OverviewGrid::Shutdown");

  EndNudge();

  auto* root_controller = RootWindowController::ForWindow(root_window_);
  root_controller->EndSplitViewOverviewSession(
      GetSplitViewOverviewSessionExitPoint(overview_session_));
  SplitViewController::Get(root_window_)->RemoveObserver(this);
  if (auto* animator = root_controller->GetScreenRotationAnimator()) {
    animator->RemoveObserver(this);
  }

  Shell* shell = Shell::Get();
  shell->wallpaper_controller()->RemoveObserver(this);
  grid_event_handler_.reset();

  if (IsShowingSavedDeskLibrary())
    HideSavedDeskLibrary(/*exit_overview=*/true);

  bool has_non_cover_animating = false;
  int animate_count = 0;

  for (const auto& window : window_list_) {
    if (window->should_animate_when_exiting() && !has_non_cover_animating) {
      has_non_cover_animating |=
          !CanCoverAvailableWorkspace(window->GetWindow());
      animate_count++;
    }
    window->Shutdown();
  }

  const bool in_split_view =
      SplitViewController::Get(root_window_)->InSplitViewMode();
  // OverviewGrid in splitscreen does not include the window to be activated.
  if (!window_list_.empty() || in_split_view) {
    const bool minimized_in_tablet =
        overview_session_->enter_exit_overview_type() ==
        OverviewEnterExitType::kFadeOutExit;
    const bool single_animation_in_clamshell =
        (animate_count == 1 && !has_non_cover_animating) &&
        !shell->tablet_mode_controller()->InTabletMode();
    // The following instance self-destructs when shutdown animation ends.
    new ShutdownAnimationMetricsTrackerObserver(
        root_window_->layer()->GetCompositor(), in_split_view,
        single_animation_in_clamshell, minimized_in_tablet);
  }

  drop_target_ = nullptr;
  window_list_.clear();

  overview_session_ = nullptr;

  if (no_windows_widget_) {
    if (exit_type == OverviewEnterExitType::kImmediateExit) {
      ImmediatelyCloseWidgetOnExit(std::move(no_windows_widget_));
      return;
    }

    // Fade out the no windows widget. This animation continues past the
    // lifetime of |this|.
    FadeOutWidgetFromOverview(std::move(no_windows_widget_),
                              OVERVIEW_ANIMATION_RESTORE_WINDOW);
  }

  // After this, the desk bar widget will not be owned by this overview grid
  // anymore.
  if (desks_widget_) {
    if (chromeos::features::IsJellyrollEnabled() &&
        exit_type != OverviewEnterExitType::kImmediateExit) {
      PerformDeskBarSlideAnimation(std::move(desks_widget_),
                                   desks_bar_view_->IsZeroState());
    }
    desks_widget_.reset();
    desks_bar_view_ = nullptr;
  }
}

void OverviewGrid::PrepareForOverview() {
  if (ShouldImmediatelyInitDeskBar(this)) {
    MaybeInitDesksWidget();
  }

  for (const auto& window : window_list_) {
    window->PrepareForOverview();
  }

  SplitViewController::Get(root_window_)->AddObserver(this);
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    if (auto* animator = RootWindowController::ForWindow(root_window_)
                             ->GetScreenRotationAnimator()) {
      animator->AddObserver(this);
    }
  }

  grid_event_handler_ = std::make_unique<OverviewGridEventHandler>(this);
  Shell::Get()->wallpaper_controller()->AddObserver(this);
}

void OverviewGrid::PositionWindowsContinuously(float y_offset) {
  const float scroll_ratio = y_offset / WmGestureHandler::kVerticalThresholdDp;

  // Move the desks bar up/down.
  if (auto* desks_bar = desks_bar_view()) {
    desks_bar->layer()->SetTransform(gfx::Transform::MakeTranslation(
        0, desks_bar->height() * (scroll_ratio - 1)));
  }

  // Compute and adjust the "No recent items" label.
  if (no_windows_widget_) {
    no_windows_widget_->GetLayer()->SetOpacity(
        std::clamp(0.01f, scroll_ratio, 1.f));
  }

  if (!cached_transforms_.empty()) {
    // TODO(http://b/297923747): Integrate continuous animation with snap
    // groups.
    for (const auto& [overview_item, transform] : cached_transforms_) {
      overview_item->OnOverviewItemContinuousScroll(transform, scroll_ratio);
    }
    return;
  }

  if (window_list_.empty()) {
    return;
  }

  // The first time we call this function we want to compute the target
  // transforms.
  const std::vector<gfx::RectF> target_rects =
      ShouldUseScrollingLayout(/*ignored_items_size=*/0u)
          ? GetWindowRectsForScrollingLayout({})
          : GetWindowRects({});
  CHECK_EQ(window_list_.size(), target_rects.size());

  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItemBase* overview_item = window_list_[i].get();
    cached_transforms_[overview_item] =
        overview_item->ComputeTargetTransform(target_rects[i]);

    if (WindowState::Get(overview_item->GetWindow())->IsMinimized()) {
      overview_item->SetBounds(target_rects[i], OVERVIEW_ANIMATION_NONE);
    } else {
      // Keep the overview header, backdrop, etc. and rounded corners and shadow
      // hidden during the trackpad swipe. They will be shown after the swipe is
      // completed.
      overview_item->item_widget()->GetLayer()->SetOpacity(0.f);
      overview_item->UpdateRoundedCornersAndShadow();
    }
  }

  // When starting a continuous scroll to EXIT overview mode, hide the save
  // desk button immediately.
  // When starting a continuous scroll to ENTER overview mode, the save desk
  // button will be shown once overview mode is fully entered.
  if (IsSaveDeskButtonContainerVisible()) {
    UpdateSaveDeskButtons();
  }
}

void OverviewGrid::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItemBase*>& ignored_items,
    OverviewTransition transition) {
  if (!overview_session_ || suspend_reposition_ || window_list_.empty())
    return;

  DCHECK_NE(transition, OverviewTransition::kExit);

  std::vector<gfx::RectF> rects =
      ShouldUseScrollingLayout(ignored_items.size())
          ? GetWindowRectsForScrollingLayout(ignored_items)
          : GetWindowRects(ignored_items);

  if (transition == OverviewTransition::kEnter) {
    CalculateWindowListAnimationStates(/*selected_item=*/nullptr, transition,
                                       rects);
  }

  // Position the windows centering the left-aligned rows vertically. Do not
  // position items in |ignored_items|.
  OverviewAnimationType animation_type = OVERVIEW_ANIMATION_NONE;
  switch (transition) {
    case OverviewTransition::kEnter: {
      const bool entering_from_home =
          overview_session_->enter_exit_overview_type() ==
          OverviewEnterExitType::kFadeInEnter;
      animation_type = entering_from_home
                           ? OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER
                           : OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER;
      break;
    }
    case OverviewTransition::kInOverview:
      animation_type = OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW;
      break;
    case OverviewTransition::kExit:
      NOTREACHED();
  }

  int animate_count = 0;
  bool has_non_cover_animating = false;
  std::vector<OverviewAnimationType> animation_types(rects.size());

  const bool can_do_spawn_animation =
      animate && transition == OverviewTransition::kInOverview;

  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItemBase* window_item = window_list_[i].get();
    if (ShouldExcludeItemFromGridLayout(window_item, ignored_items)) {
      rects[i].SetRect(0, 0, 0, 0);
      continue;
    }

    // Calculate if each window item needs animation.
    bool should_animate_item = animate;
    // If we're in entering overview process, not all window items in the grid
    // might need animation even if the grid needs animation.
    if (animate && transition == OverviewTransition::kEnter) {
      should_animate_item = window_item->should_animate_when_entering();
      if (should_animate_item && !has_non_cover_animating) {
        has_non_cover_animating |=
            !CanCoverAvailableWorkspace(window_item->GetWindow());
        ++animate_count;
      }
    }

    if (can_do_spawn_animation && window_item->should_use_spawn_animation())
      animation_type = OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW;

    animation_types[i] =
        should_animate_item ? animation_type : OVERVIEW_ANIMATION_NONE;
  }

  if (animate && transition == OverviewTransition::kEnter &&
      !window_list_.empty()) {
    bool single_animation_in_clamshell =
        animate_count == 1 && !has_non_cover_animating &&
        !Shell::Get()->tablet_mode_controller()->InTabletMode();
    bool minimized_in_tablet = overview_session_->enter_exit_overview_type() ==
                               OverviewEnterExitType::kFadeInEnter;
    metrics_tracker_ = std::make_unique<OverviewEnterMetricsTracker>(
        window_list_[0]->GetWindow()->layer()->GetCompositor(),
        SplitViewController::Get(root_window_)->InSplitViewMode(),
        single_animation_in_clamshell, minimized_in_tablet);
  }

  // Apply the animation after creating metrics_tracker_ so that unit test
  // can correctly count the measure requests.
  for (size_t i = 0; i < window_list_.size(); ++i) {
    if (const auto& rect = rects[i]; !rect.IsEmpty()) {
      window_list_[i].get()->SetBounds(rect, animation_types[i]);
    }
  }

  UpdateSaveDeskButtons();

  // This is a no-op if the feature ContinuousOverviewScrollAnimation is not
  // enabled. Once windows are placed at their final positions, clear transforms
  // so that they get re-calculated when a continuous downward scroll begins.
  cached_transforms_.clear();
}

OverviewItemBase* OverviewGrid::GetOverviewItemContaining(
    const aura::Window* window) const {
  for (const auto& window_item : window_list_) {
    if (window_item && window_item->Contains(window)) {
      return window_item.get();
    }
  }

  return nullptr;
}

void OverviewGrid::AddItem(
    aura::Window* window,
    bool reposition,
    bool animate,
    const base::flat_set<OverviewItemBase*>& ignored_items,
    size_t index,
    bool use_spawn_animation,
    bool restack) {
  CHECK(!GetOverviewItemContaining(window));
  CHECK_LE(index, window_list_.size());

  std::unique_ptr<OverviewItemBase> overview_item_base =
      OverviewItemBase::Create(window, overview_session_, this);
  UpdateNumSavedDeskUnsupportedWindows(overview_item_base->GetWindows(),
                                       /*increment=*/true);
  window_list_.insert(window_list_.begin() + index,
                      std::move(overview_item_base));

  if (overview_session_)
    overview_session_->UpdateFrameThrottling();

  auto* item = window_list_[index].get();
  item->PrepareForOverview();

  // No animations if the saved desk grid is showing, even if `animate` is true.
  const bool should_animate = animate && !IsShowingSavedDeskLibrary();

  if (should_animate && use_spawn_animation && reposition) {
    item->set_should_use_spawn_animation(true);
  } else {
    // The item is added after overview enter animation is complete, so
    // just call OnStartingAnimationComplete() only if we won't animate it with
    // with the spawn animation. Otherwise, OnStartingAnimationComplete() will
    // be called when the spawn-item-animation completes (See
    // OverviewItem::OnItemSpawnedAnimationCompleted()).
    item->OnStartingAnimationComplete();
  }

  if (restack) {
    if (reposition && should_animate)
      item->set_should_restack_on_animation_end(true);
    else
      item->Restack();
  }
  if (reposition)
    PositionWindows(should_animate, ignored_items);

  if (IsShowingSavedDeskLibrary()) {
    item->HideForSavedDeskLibrary(/*animate=*/false);
  }
}

void OverviewGrid::AppendItem(aura::Window* window,
                              bool reposition,
                              bool animate,
                              bool use_spawn_animation) {
  AddItem(window, reposition, animate, /*ignored_items=*/{},
          window_list_.size(), use_spawn_animation, /*restack=*/false);
}

void OverviewGrid::AddItemInMruOrder(aura::Window* window,
                                     bool reposition,
                                     bool animate,
                                     bool restack,
                                     bool use_spawn_animation) {
  AddItem(window, reposition, animate, /*ignored_items=*/{},
          FindInsertionIndex(window), use_spawn_animation, restack);
}

void OverviewGrid::RemoveItem(OverviewItemBase* overview_item,
                              bool item_destroying,
                              bool reposition) {
  EndNudge();

  // Use reverse iterator to be efficient when removing all.
  auto iter = base::ranges::find(base::Reversed(window_list_), overview_item,
                                 &std::unique_ptr<OverviewItemBase>::get);
  DCHECK(iter != window_list_.rend());

  UpdateNumSavedDeskUnsupportedWindows(overview_item->GetWindows(),
                                       /*increment=*/false);

  // This can also be called when shutting down |this|, at which the item will
  // be cleaning up and its associated view may be nullptr. |overview_item|
  // needs to still be in |window_list_| so we can compute what the deleted
  // index is.
  if (OverviewFocusableView* focusable_view = (*iter)->GetFocusableView();
      overview_session_ && focusable_view) {
    overview_session_->focus_cycler()->OnViewDestroyingOrDisabling(
        focusable_view);
  }

  // Erase from the list first because deleting OverviewItem can lead to
  // iterating through the |window_list_|.
  std::unique_ptr<OverviewItemBase> tmp = std::move(*iter);
  window_list_.erase(std::next(iter).base());
  tmp.reset();

  if (overview_session_)
    overview_session_->UpdateFrameThrottling();

  if (!item_destroying || !overview_session_) {
    return;
  }

  if (empty()) {
    overview_session_->OnGridEmpty();
    return;
  }

  if (reposition) {
    // Update the grid bounds if needed and reposition the windows minus the
    // currently overview dragged window, if there is one. Note: this does not
    // update the grid bounds if the window being dragged from the top or shelf,
    // the former being handled in TabletModeWindowDragDelegate's destructor.
    base::flat_set<OverviewItemBase*> ignored_items;
    OverviewItemBase* dragged_item =
        overview_session_->GetCurrentDraggedOverviewItem();
    if (dragged_item)
      ignored_items.insert(dragged_item);
    const gfx::Rect grid_bounds = GetGridBoundsInScreen(
        root_window_,
        split_view_drag_indicators_
            ? absl::make_optional(
                  split_view_drag_indicators_->current_window_dragging_state())
            : absl::nullopt,
        /*divider_changed=*/false,
        /*account_for_hotseat=*/true);
    SetBoundsAndUpdatePositions(grid_bounds, ignored_items, /*animate=*/true);
  }
}

void OverviewGrid::RemoveAllItemsForSavedDeskLaunch() {
  {
    // Wait until the end to notify content changes for all desks.
    Desk::ScopedContentUpdateNotificationDisabler desks_scoped_notify_disabler(
        /*desks=*/DesksController::Get()->desks(),
        /*notify_when_destroyed=*/true);

    for (auto& item : window_list_) {
      item->RevertHideForSavedDeskLibrary(/*animate=*/false);
      item->RestoreWindow(/*reset_transform=*/true, /*animate=*/false);
    }
  }
  window_list_.clear();
  num_incognito_windows_ = 0;
  num_unsupported_windows_ = 0;
  EnableSaveDeskButtonContainer();
}

void OverviewGrid::AddDropTargetForDraggingFromThisGrid(
    OverviewItemBase* dragged_item) {
  const size_t position = GetOverviewItemIndex(dragged_item) + 1u;
  AddDropTargetImpl(dragged_item, position, /*animate=*/false);
}

void OverviewGrid::AddDropTargetNotForDraggingFromThisGrid(
    aura::Window* dragged_window,
    bool animate) {
  const size_t position = FindInsertionIndex(dragged_window);
  AddDropTargetImpl(nullptr, position, /*animate=*/false);

  if (!animate) {
    return;
  }

  views::Widget* drop_target_widget = drop_target_->item_widget();
  drop_target_widget->SetOpacity(0.f);
  ScopedOverviewAnimationSettings settings(
      OVERVIEW_ANIMATION_DROP_TARGET_FADE,
      drop_target_widget->GetNativeWindow());
  drop_target_widget->SetOpacity(1.f);
}

void OverviewGrid::RemoveDropTarget() {
  CHECK(drop_target_);

  // Copy to a local first to avoid a dangling pointer.
  OverviewDropTarget* drop_target_ptr = std::exchange(drop_target_, nullptr);

  size_t erased_count = base::EraseIf(
      window_list_, base::MatchesUniquePtr<OverviewItemBase>(drop_target_ptr));
  CHECK_EQ(1u, erased_count);

  const size_t num_items = overview_session_->num_items();
  CHECK_GT(num_items, 0u);
  overview_session_->set_num_items(num_items - 1u);

  // Skip repositioning here. The caller is expected to call `PositionWindows()`
  // after more drag-ending cleanup in `OverviewWindowDragController`.
}

void OverviewGrid::SetBoundsAndUpdatePositions(
    const gfx::Rect& bounds_in_screen,
    const base::flat_set<OverviewItemBase*>& ignored_items,
    bool animate) {
  const bool bounds_updated = bounds_in_screen != bounds_;
  bounds_ = bounds_in_screen;
  MaybeUpdateDesksWidgetBounds();
  PositionWindows(animate, ignored_items);

  if (bounds_updated && saved_desk_library_widget_)
    saved_desk_library_widget_->SetBounds(GetGridEffectiveBounds());
}

void OverviewGrid::RearrangeDuringDrag(
    OverviewItemBase* dragged_item,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  // Update the drop target visibility according to `window_dragging_state`.
  if (drop_target_) {
    views::Widget* drop_target_widget = drop_target_->item_widget();
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_DROP_TARGET_FADE,
        drop_target_widget->GetNativeWindow());
    drop_target_widget->SetOpacity(
        GetWantedDropTargetOpacity(window_dragging_state));
  }

  // Update the grid's bounds.
  const gfx::Rect wanted_grid_bounds = GetGridBoundsInScreen(
      root_window_, absl::make_optional(window_dragging_state),
      /*divider_changed=*/false, /*account_for_hotseat=*/true);
  if (bounds_ != wanted_grid_bounds) {
    base::flat_set<OverviewItemBase*> ignored_items;
    if (dragged_item)
      ignored_items.insert(dragged_item);
    SetBoundsAndUpdatePositions(wanted_grid_bounds, ignored_items,
                                /*animate=*/true);
  }
}

void OverviewGrid::SetSplitViewDragIndicatorsDraggedWindow(
    aura::Window* dragged_window) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetDraggedWindow(dragged_window);
}

void OverviewGrid::SetSplitViewDragIndicatorsWindowDraggingState(
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetWindowDraggingState(window_dragging_state);
}

bool OverviewGrid::MaybeUpdateDesksWidgetBounds() {
  if (!desks_widget_)
    return false;

  const gfx::Rect desks_widget_bounds = GetDesksWidgetBounds();
  if (desks_widget_bounds != desks_widget_->GetWindowBoundsInScreen()) {
    // Note that the desks widget window is placed on the active desk container,
    // which has the kUsesScreenCoordinatesKey property set to true, and hence
    // we use the screen coordinates when positioning the desks widget.
    //
    // On certain display zooms, the requested |desks_widget_bounds| may differ
    // than the current screen bounds of the desks widget by 1dp, but internally
    // it will end up being the same and therefore a layout may not be
    // triggered. This can cause mini views not to show up at all. We must
    // guarantee that a layout will always occur by invalidating the layout.
    // See crbug.com/1056371 for more details.
    desks_bar_view_->InvalidateLayout();
    desks_widget_->SetBounds(desks_widget_bounds);
    return true;
  }
  return false;
}

void OverviewGrid::UpdateDropTargetBackgroundVisibility(
    OverviewItemBase* dragged_item,
    const gfx::PointF& location_in_screen) {
  CHECK(drop_target_);
  drop_target_->UpdateBackgroundVisibility(
      gfx::ToRoundedPoint(location_in_screen));
}

void OverviewGrid::OnOverviewItemDragStarted(OverviewItemBase* item) {
  CommitNameChanges();
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnOverviewItemDragStarted(item);
}

void OverviewGrid::OnOverviewItemDragEnded(bool snap) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnOverviewItemDragEnded(snap);
}

void OverviewGrid::OnWindowDragStarted(aura::Window* dragged_window,
                                       bool animate) {
  dragged_window_ = dragged_window;
  AddDropTargetNotForDraggingFromThisGrid(dragged_window, animate);
  // Stack the |dragged_window| at top during drag.
  dragged_window->parent()->StackChildAtTop(dragged_window);
  // Called to set caption and title visibility during dragging.
  OnOverviewItemDragStarted(/*item=*/nullptr);
}

void OverviewGrid::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen,
    SplitViewDragIndicators::WindowDraggingState window_dragging_state) {
  DCHECK_EQ(dragged_window_, dragged_window);
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);

  RearrangeDuringDrag(nullptr, window_dragging_state);
  UpdateDropTargetBackgroundVisibility(nullptr, location_in_screen);
}

void OverviewGrid::OnWindowDragEnded(aura::Window* dragged_window,
                                     const gfx::PointF& location_in_screen,
                                     bool should_drop_window_into_overview,
                                     bool snap) {
  DCHECK_EQ(dragged_window_, dragged_window);
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  CHECK(drop_target_);
  dragged_window_ = nullptr;

  // Add the dragged window into drop target in overview if
  // |should_drop_window_into_overview| is true.
  if (should_drop_window_into_overview) {
    AddDraggedWindowIntoOverviewOnDragEnd(dragged_window);
  }

  RemoveDropTarget();

  // Called to reset caption and title visibility after dragging.
  OnOverviewItemDragEnded(snap);

  // Update the grid bounds and reposition windows. Since the grid bounds might
  // be updated based on the preview area during drag, but the window finally
  // didn't be snapped to the preview area.
  RefreshGridBounds(/*animate=*/true);
}

void OverviewGrid::MergeWindowIntoOverviewForWebUITabStrip(
    aura::Window* dragged_window) {
  AddDraggedWindowIntoOverviewOnDragEnd(dragged_window);
  RefreshGridBounds(/*animate=*/true);
}

void OverviewGrid::SetVisibleDuringWindowDragging(bool visible, bool animate) {
  for (const auto& window_item : window_list_) {
    window_item->SetVisibleDuringItemDragging(visible, animate);
  }

  // Update |desks_widget_|.
  if (desks_widget_) {
    ui::Layer* layer = desks_widget_->GetNativeWindow()->layer();
    float new_opacity = visible ? 1.f : 0.f;
    if (layer->GetTargetOpacity() == new_opacity)
      return;

    if (animate) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG,
          desks_widget_->GetNativeWindow());
      layer->SetOpacity(new_opacity);
    } else {
      layer->SetOpacity(new_opacity);
    }
  }
}

void OverviewGrid::OnDisplayMetricsChanged() {
  if (split_view_drag_indicators_)
    split_view_drag_indicators_->OnDisplayBoundsChanged();

  UpdateCannotSnapWarningVisibility(/*animate=*/true);

  // In case of split view mode, the grid bounds and item positions will be
  // updated in |OnSplitViewDividerPositionChanged|.
  if (SplitViewController::Get(root_window_)->InSplitViewMode())
    return;
  RefreshGridBounds(/*animate=*/false);
}

void OverviewGrid::OnUserWorkAreaInsetsChanged(aura::Window* root_window) {
  DCHECK_EQ(root_window, root_window_);
  if (!desks_widget_)
    return;

  RefreshGridBounds(/*animate=*/false);
}

void OverviewGrid::OnStartingAnimationComplete(bool canceled) {
  metrics_tracker_.reset();
  if (canceled)
    return;

  MaybeInitDesksWidget();

  UpdateSaveDeskButtons();

  for (auto& window : window_list())
    window->OnStartingAnimationComplete();
}

void OverviewGrid::CalculateWindowListAnimationStates(
    OverviewItemBase* selected_item,
    OverviewTransition transition,
    const std::vector<gfx::RectF>& target_bounds) {
  using OverviewTransition = OverviewTransition;

  // Sanity checks to enforce assumptions used in later codes.
  switch (transition) {
    case OverviewTransition::kEnter:
      DCHECK_EQ(target_bounds.size(), window_list_.size());
      break;
    case OverviewTransition::kExit:
      DCHECK(target_bounds.empty());
      break;
    default:
      NOTREACHED();
  }

  // On top items are items that are higher up on the z-order, or in the always
  // on top or float containers.
  auto is_on_top_item = [](OverviewItemBase* item) -> bool {
    CHECK(item);

    if (item->GetWindow()->GetProperty(aura::client::kZOrderingKey) !=
        ui::ZOrderLevel::kNormal) {
      return true;
    }

    aura::Window* parent = item->GetWindow()->parent();
    aura::Window* root = parent->GetRootWindow();
    return parent == root->GetChildById(kShellWindowId_AlwaysOnTopContainer) ||
           parent == root->GetChildById(kShellWindowId_FloatContainer);
  };

  // Create a copy of `window_list_` which has the selected item and on top
  // windows in the front.
  std::vector<OverviewItemBase*> on_top_items;
  std::vector<OverviewItemBase*> regular_items;
  for (const std::unique_ptr<OverviewItemBase>& item : window_list_) {
    OverviewItemBase* item_ptr = item.get();
    CHECK(item_ptr);
    // Skip the selected item; it will be inserted into the front. Skip the drop
    // target, it is translucent and doesn't factor into any of these
    // calculations.
    if (item_ptr == selected_item || item_ptr == drop_target_) {
      continue;
    }

    if (is_on_top_item(item_ptr))
      on_top_items.push_back(item_ptr);
    else
      regular_items.push_back(item_ptr);
  }

  // Construct `items` so they are ordered like so.
  //   1) Selected window which is on top.
  //   2) On top windows.
  //   3) Selected window which is not on top.
  //   4) Regular window.
  // Windows in the same group maintain their ordering from `window_list`.
  std::vector<OverviewItemBase*> items;
  if (selected_item && is_on_top_item(selected_item))
    items.insert(items.begin(), selected_item);
  items.insert(items.end(), on_top_items.begin(), on_top_items.end());
  if (selected_item && !is_on_top_item(selected_item))
    items.insert(items.end(), selected_item);
  items.insert(items.end(), regular_items.begin(), regular_items.end());

  SkRegion occluded_region;
  auto* split_view_controller = SplitViewController::Get(root_window_);
  if (split_view_controller->InSplitViewMode()) {
    // Snapped windows and the split view divider are not included in
    // |target_bounds| or |window_list_|, but can occlude other windows, so add
    // them manually to |region| here.
    SkIRect snapped_window_bounds = gfx::RectToSkIRect(
        split_view_controller->GetDefaultSnappedWindow()->GetBoundsInScreen());
    occluded_region.op(snapped_window_bounds, SkRegion::kUnion_Op);

    auto* divider = split_view_controller->split_view_divider();
    if (divider) {
      aura::Window* divider_window =
          divider->divider_widget()->GetNativeWindow();
      SkIRect divider_bounds =
          gfx::RectToSkIRect(divider_window->GetBoundsInScreen());
      occluded_region.op(divider_bounds, SkRegion::kUnion_Op);
    }
  }

  // TODO(sammiequon): Investigate the bounds used here and if we need to
  // consider tucked windows.
  gfx::Rect grid_bounds = GetGridEffectiveBounds();
  for (size_t i = 0; i < items.size(); ++i) {
    const bool minimized =
        WindowState::Get(items[i]->GetWindow())->IsMinimized();
    bool src_occluded = minimized;
    bool dst_occluded = false;
    gfx::Rect src_bounds_temp =
        minimized ? gfx::Rect()
                  : items[i]->GetWindow()->GetBoundsInRootWindow();
    if (!src_bounds_temp.IsEmpty()) {
      if (transition == OverviewTransition::kEnter &&
          Shell::Get()->tablet_mode_controller()->InTabletMode()) {
        BackdropController* backdrop_controller =
            GetActiveWorkspaceController(root_window_)
                ->layout_manager()
                ->backdrop_controller();
        if (backdrop_controller->GetTopmostWindowWithBackdrop() ==
            items[i]->GetWindow()) {
          src_bounds_temp = screen_util::GetDisplayWorkAreaBoundsInParent(
              items[i]->GetWindow());
        }
      } else if (transition == OverviewTransition::kExit) {
        // On exiting overview, |GetBoundsInRootWindow()| will have the overview
        // translation applied to it, so use |bounds()| and
        // |ConvertRectToScreen()| to get the true target bounds.
        src_bounds_temp = items[i]->GetWindow()->bounds();
        wm::ConvertRectToScreen(items[i]->root_window(), &src_bounds_temp);
      }
      src_bounds_temp.Intersect(grid_bounds);
    }

    // The bounds of of the destination may be partially or fully offscreen.
    // Partially offscreen rects should be clipped so the onscreen portion is
    // treated normally. Fully offscreen rects (intersection with the screen
    // bounds is empty) should never be animated.
    gfx::Rect dst_bounds_temp = gfx::ToEnclosedRect(
        transition == OverviewTransition::kEnter ? target_bounds[i]
                                                 : items[i]->target_bounds());
    dst_bounds_temp.Intersect(grid_bounds);
    if (dst_bounds_temp.IsEmpty()) {
      items[i]->set_should_animate_when_entering(false);
      items[i]->set_should_animate_when_exiting(false);
      continue;
    }

    SkIRect src_bounds = gfx::RectToSkIRect(src_bounds_temp);
    SkIRect dst_bounds = gfx::RectToSkIRect(dst_bounds_temp);
    if (!occluded_region.isEmpty()) {
      src_occluded |=
          (!src_bounds.isEmpty() && occluded_region.contains(src_bounds));
      dst_occluded |= occluded_region.contains(dst_bounds);
    }

    // Add |src_bounds| to our region if it is not empty (minimized window).
    if (!src_bounds.isEmpty())
      occluded_region.op(src_bounds, SkRegion::kUnion_Op);

    const bool should_animate = !(src_occluded && dst_occluded);
    if (transition == OverviewTransition::kEnter)
      items[i]->set_should_animate_when_entering(should_animate);
    else if (transition == OverviewTransition::kExit)
      items[i]->set_should_animate_when_exiting(should_animate);
  }
}

void OverviewGrid::SetWindowListNotAnimatedWhenExiting() {
  should_animate_when_exiting_ = false;
  for (const auto& item : window_list_)
    item->set_should_animate_when_exiting(false);
}

void OverviewGrid::StartNudge(OverviewItemBase* item) {
  // When there is one window left, there is no need to nudge.
  if (window_list_.size() <= 1) {
    nudge_data_.clear();
    return;
  }

  // If any of the items are being animated to close, do not nudge any windows
  // otherwise we have to deal with potential items getting removed from
  // |window_list_| midway through a nudge.
  for (const auto& window_item : window_list_) {
    if (window_item->animating_to_close()) {
      nudge_data_.clear();
      return;
    }
  }

  DCHECK(item);

  // Get the bounds of the windows currently, and the bounds if |item| were to
  // be removed.
  std::vector<gfx::RectF> src_rects;
  for (const auto& window_item : window_list_)
    src_rects.push_back(window_item->target_bounds());

  std::vector<gfx::RectF> dst_rects = GetWindowRects({item});

  const size_t index = GetOverviewItemIndex(item);

  // Returns a vector of integers indicating which row the item is in. |index|
  // is the index of the element which is going to be deleted and should not
  // factor into calculations. The call site should mark |index| as -1 if it
  // should not be used. The item at |index| is marked with a 0. The heights of
  // items are all set to the same value so a new row is determined if the y
  // value has changed from the previous item.
  auto get_rows = [](const std::vector<gfx::RectF>& bounds_list, size_t index) {
    std::vector<int> row_numbers;
    int current_row = 1;
    float last_y = 0;
    for (size_t i = 0; i < bounds_list.size(); ++i) {
      if (i == index) {
        row_numbers.push_back(0);
        continue;
      }

      // Update |current_row| if the y position has changed (heights are all
      // equal in overview, so a new y position indicates a new row).
      if (last_y != 0 && last_y != bounds_list[i].y())
        ++current_row;

      row_numbers.push_back(current_row);
      last_y = bounds_list[i].y();
    }

    return row_numbers;
  };

  std::vector<int> src_rows = get_rows(src_rects, -1);
  std::vector<int> dst_rows = get_rows(dst_rects, index);

  // Do nothing if the number of rows change.
  if (dst_rows.back() != 0 && src_rows.back() != dst_rows.back())
    return;
  size_t second_last_index = src_rows.size() - 2;
  if (dst_rows.back() == 0 &&
      src_rows[second_last_index] != dst_rows[second_last_index]) {
    return;
  }

  // Do nothing if the last item from the previous row will drop onto the
  // current row, this will cause the items in the current row to shift to the
  // right while the previous item stays in the previous row, which looks weird.
  if (src_rows[index] > 1) {
    // Find the last item from the previous row.
    size_t previous_row_last_index = index;
    while (src_rows[previous_row_last_index] == src_rows[index]) {
      --previous_row_last_index;
    }

    // Early return if the last item in the previous row changes rows.
    if (src_rows[previous_row_last_index] != dst_rows[previous_row_last_index])
      return;
  }

  // Helper to check whether the item at |item_index| will be nudged.
  auto should_nudge = [&src_rows, &dst_rows, &index](size_t item_index) {
    // Out of bounds.
    if (item_index >= src_rows.size())
      return false;

    // Nudging happens when the item stays on the same row and is also on the
    // same row as the item to be deleted was.
    if (dst_rows[item_index] == src_rows[index] &&
        dst_rows[item_index] == src_rows[item_index]) {
      return true;
    }

    return false;
  };

  // Starting from |index| go up and down while the nudge condition returns
  // true.
  std::vector<int> affected_indexes;
  size_t loop_index;

  if (index > 0) {
    loop_index = index - 1;
    while (should_nudge(loop_index)) {
      affected_indexes.push_back(loop_index);
      --loop_index;
    }
  }

  loop_index = index + 1;
  while (should_nudge(loop_index)) {
    affected_indexes.push_back(loop_index);
    ++loop_index;
  }

  // Populate |nudge_data_| with the indexes in |affected_indexes| and their
  // respective source and destination bounds.
  nudge_data_.resize(affected_indexes.size());
  for (size_t i = 0; i < affected_indexes.size(); ++i) {
    const size_t new_index = static_cast<size_t>(affected_indexes[i]);
    nudge_data_[i] = {.index = new_index,
                      .src = src_rects[new_index],
                      .dst = dst_rects[new_index]};
  }
}

void OverviewGrid::UpdateNudge(OverviewItemBase* item, double value) {
  for (const OverviewNudgeData& data : nudge_data_) {
    DCHECK_LT(data.index, window_list_.size());

    OverviewItemBase* nudged_item = window_list_[data.index].get();
    double nudge_param = value * value / 30.0;
    nudge_param = std::clamp(nudge_param, 0.0, 1.0);
    gfx::RectF bounds =
        gfx::Tween::RectFValueBetween(nudge_param, data.src, data.dst);
    nudged_item->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
  }
}

void OverviewGrid::EndNudge() {
  nudge_data_.clear();
}

aura::Window* OverviewGrid::GetTargetWindowOnLocation(
    const gfx::PointF& location_in_screen,
    OverviewItemBase* ignored_item) {
  for (std::unique_ptr<OverviewItemBase>& item : window_list_) {
    if (item.get() == ignored_item)
      continue;
    if (item->target_bounds().Contains(location_in_screen))
      return item->GetWindow();
  }
  return nullptr;
}

bool OverviewGrid::IsDesksBarViewActive() const {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  // The desk bar view is not active if there is only a single desk when
  // overview is started. Or when the desks bar view has been created and in
  // zero state.
  return DesksController::Get()->desks().size() > 1 ||
         (desks_bar_view_ && !desks_bar_view_->IsZeroState());
}

gfx::Rect OverviewGrid::GetGridEffectiveBounds() const {
  // There's an edge case where is in tablet mode, there're more than one desk,
  // after entering overview mode, deleting desks to just keep one, even though
  // there's only one desk now in tablet mode, the desks bar will stay. That's
  // why we need to check the existence of `desks_bar_view_` here.
  if (!desks_bar_view_ && !desks_util::ShouldDesksBarBeCreated())
    return bounds_;

  gfx::Rect effective_bounds = bounds_;
  effective_bounds.Inset(gfx::Insets::TLBR(GetDesksBarHeight(), 0, 0, 0));
  return effective_bounds;
}

gfx::Insets OverviewGrid::GetGridInsets() const {
  return GetGridInsetsImpl(GetGridEffectiveBounds());
}

bool OverviewGrid::IntersectsWithDesksBar(const gfx::Point& screen_location,
                                          bool update_desks_bar_drag_details,
                                          bool for_drop) {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  const bool dragged_item_over_bar =
      desks_widget_->GetWindowBoundsInScreen().Contains(screen_location);
  if (update_desks_bar_drag_details) {
    desks_bar_view_->SetDragDetails(screen_location,
                                    !for_drop && dragged_item_over_bar);
  }
  return dragged_item_over_bar;
}

bool OverviewGrid::MaybeDropItemOnDeskMiniViewOrNewDeskButton(
    const gfx::Point& screen_location,
    OverviewItemBase* dragged_item) {
  CHECK(desks_util::ShouldDesksBarBeCreated());

  const bool has_windows_visible_on_all_desks =
      dragged_item->HasVisibleOnAllDesksWindow();

  // End the drag for the LegacyDeskBarView.
  if (!IntersectsWithDesksBar(screen_location,
                              /*update_desks_bar_drag_details=*/
                              !has_windows_visible_on_all_desks,
                              /*for_drop=*/true)) {
    return false;
  }

  if (has_windows_visible_on_all_desks) {
    // Show toast since items that are visible on all desks should not be able
    // to be unassigned during overview.
    // TODO(b/306034162): Consider updating the string for the toast with the
    // existence of group item.
    Shell::Get()->toast_manager()->Show(
        ToastData(kMoveVisibleOnAllDesksWindowToastId,
                  ToastCatalogName::kMoveVisibleOnAllDesksWindow,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_OVERVIEW_VISIBLE_ON_ALL_DESKS_TOAST)));
    return false;
  }

  auto* desks_controller = DesksController::Get();

  auto move_windows_to_target_desk = [&](Desk* target_desk) -> bool {
    bool did_any_window_move = false;
    for (auto* dragged_window : dragged_item->GetWindows()) {
      if (!desks_controller->MoveWindowFromActiveDeskTo(
              dragged_window, target_desk, root_window_,
              DesksMoveWindowFromActiveDeskSource::kDragAndDrop)) {
        CHECK(!did_any_window_move);
        return false;
      }
      did_any_window_move = true;
    }
    return true;
  };

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();
  for (auto* mini_view : desks_bar_view_->mini_views()) {
    if (!mini_view->IsPointOnMiniView(screen_location))
      continue;

    Desk* const target_desk = mini_view->desk();
    if (target_desk == desks_controller->active_desk())
      return false;

    if (is_jellyroll_enabled) {
      // Make sure that new desk button goes back to the expanded state after
      // the window is dropped on an existing desk.
      desks_bar_view_->UpdateDeskIconButtonState(
          desks_bar_view_->new_desk_button(),
          /*target_state=*/CrOSNextDeskIconButton::State::kExpanded);
    }

    return move_windows_to_target_desk(target_desk);
  }

  if (!desks_controller->CanCreateDesks()) {
    return false;
  }

  const bool is_point_on_new_desk_button =
      is_jellyroll_enabled
          ? desks_bar_view_->new_desk_button()->IsPointOnButton(screen_location)
          : desks_bar_view_->expanded_state_new_desk_button()->IsPointOnButton(
                screen_location);

  if (!is_point_on_new_desk_button) {
    return false;
  }

  desks_bar_view_->OnNewDeskButtonPressed(
      DesksCreationRemovalSource::kDragToNewDeskButton);

  return move_windows_to_target_desk(desks_controller->desks().back().get());
}

void OverviewGrid::MaybeExpandDesksBarView(const gfx::PointF& screen_location) {
  if (desks_bar_view_ && desks_bar_view_->IsZeroState()) {
    if ((gfx::ToRoundedPoint(screen_location) -
         desks_bar_view_->zero_state_new_desk_button()
             ->GetBoundsInScreen()
             .CenterPoint())
            .LengthSquared() <=
        kExpandDesksBarThreshold * kExpandDesksBarThreshold) {
      desks_bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                          /*expanding_bar_view=*/true);
    }
  }
}

void OverviewGrid::MaybeShrinkDesksBarView() {
  if (desks_bar_view_ && !desks_bar_view_->IsZeroState() &&
      !IsShowingSavedDeskLibrary() &&
      desks_bar_view_->mini_views().size() == 1) {
    desks_bar_view_->SwitchToZeroState();
  }
}

void OverviewGrid::StartScroll() {
  OverviewController::Get()->PauseOcclusionTracker();

  // Users are not allowed to scroll past the leftmost or rightmost bounds of
  // the items on screen in the grid. |scroll_offset_min_| is the amount needed
  // to fit the rightmost window into |total_bounds|. The max is zero which is
  // default because windows are aligned to the left from the beginning.
  gfx::Rect total_bounds = GetGridEffectiveBounds();
  total_bounds.Inset(GetGridInsetsImpl(total_bounds));

  float rightmost_window_right = 0;
  for (const auto& item : window_list_) {
    const gfx::RectF bounds = item->target_bounds();
    if (rightmost_window_right < bounds.right()) {
      rightmost_window_right = bounds.right();
    }

    item->set_scrolling_bounds(bounds);
  }

  // |rightmost_window_right| may have been modified by an earlier scroll.
  // |scroll_offset_| is added to adjust for that.
  rightmost_window_right -= scroll_offset_;
  scroll_offset_min_ = total_bounds.right() - rightmost_window_right;
  if (scroll_offset_min_ > 0.f)
    scroll_offset_min_ = 0.f;

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      const_cast<ui::Compositor*>(root_window()->layer()->GetCompositor()),
      kOverviewScrollHistogram, kOverviewScrollMaxLatencyHistogram);
}

bool OverviewGrid::UpdateScrollOffset(float delta) {
  float new_scroll_offset = scroll_offset_;
  new_scroll_offset += delta;
  new_scroll_offset = std::clamp(new_scroll_offset, scroll_offset_min_, 0.f);

  // For flings, we want to return false if we hit one of the edges, which is
  // when |new_scroll_offset| is exactly 0.f or |scroll_offset_min_|.
  const bool in_range =
      new_scroll_offset < 0.f && new_scroll_offset > scroll_offset_min_;
  if (new_scroll_offset == scroll_offset_)
    return in_range;

  // Update the bounds of the items which are currently visible on screen.
  for (const auto& item : window_list_) {
    absl::optional<gfx::RectF> scrolling_bounds_optional =
        item->scrolling_bounds();
    // Scrolling bounds may not be set if the item was added after scrolling
    // started (i.e. another desk was combined into the active desk).
    if (!scrolling_bounds_optional) {
      continue;
    }
    const gfx::RectF previous_bounds = scrolling_bounds_optional.value();
    gfx::RectF new_bounds = previous_bounds;
    new_bounds.Offset(new_scroll_offset - scroll_offset_, 0.f);
    item->set_scrolling_bounds(new_bounds);
    if (gfx::RectF(GetGridEffectiveBounds()).Intersects(new_bounds) ||
        gfx::RectF(GetGridEffectiveBounds()).Intersects(previous_bounds)) {
      item->SetBounds(new_bounds, OVERVIEW_ANIMATION_NONE);
    }
  }

  scroll_offset_ = new_scroll_offset;

  DCHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();
  return in_range;
}

void OverviewGrid::EndScroll() {
  OverviewController::Get()->UnpauseOcclusionTracker(
      kOcclusionUnpauseDurationForScroll);
  for (const auto& item : window_list_)
    item->set_scrolling_bounds(absl::nullopt);
  presentation_time_recorder_.reset();

  if (!overview_session_->is_shutting_down())
    PositionWindows(/*animate=*/false);
}

int OverviewGrid::CalculateWidthAndMaybeSetUnclippedBounds(
    OverviewItemBase* item,
    int height) {
  gfx::SizeF target_size = item->GetWindowsUnionScreenBounds().size();
  float scale = item->GetItemScale(height);
  OverviewGridWindowFillMode grid_fill_mode = item->GetWindowDimensionsType();

  // The drop target, unlike the other windows has its bounds set directly, so
  // `GetWindowsUnionScreenBounds()` won't return the value we want. Instead,
  // get the scale from the window(s) it was meant to be a placeholder for.
  if (drop_target_ == item) {
    auto* window_drag_controller = overview_session_->window_drag_controller();
    OverviewItemBase* grid_dragged_item =
        window_drag_controller ? window_drag_controller->item() : nullptr;
    aura::Window::Windows dragged_windows =
        GetWindowsAssociatedWithDragging(grid_dragged_item, dragged_window_);
    if (AreDraggedWindowsValid(dragged_windows)) {
      const gfx::Size work_area_size =
          screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
              root_window_)
              .size();
      if (AreAllWindowsMaximized(dragged_windows)) {
        // When dragging a maximized window across displays, when dragging over
        // this grid, the drop target size should reflect the maximized window
        // size on this grid's display (i.e. this display's work area size)
        // which can be different than the source display's work area size.
        grid_fill_mode = ScopedOverviewTransformWindow::GetWindowDimensionsType(
            work_area_size);
        target_size = gfx::SizeF(work_area_size);
      } else {
        // If the drag started from a different root window, `dragged_windows`
        // may not fit into the work area of `root_window_`. Then if the
        // `dragged_windows` is dropped into this grid, `dragged_windows` will
        // shrink to fit into this work area. The drop target shall reflect
        // that.
        gfx::Size dragged_item_size =
            GetTotalDraggedWindowsSize(dragged_windows);
        dragged_item_size.SetToMin(work_area_size);
        grid_fill_mode = ScopedOverviewTransformWindow::GetWindowDimensionsType(
            dragged_item_size);
        target_size = GetTotalUnionSizeIncludingTransients(dragged_windows);
        target_size.SetToMin(gfx::SizeF(work_area_size));
      }

      scale = ScopedOverviewTransformWindow::GetItemScale(
          target_size.height(), height, GetTopViewInset(dragged_windows),
          kHeaderHeightDp);
    }
  }

  int width = std::max(1, base::ClampFloor(target_size.width() * scale));
  switch (grid_fill_mode) {
    case OverviewGridWindowFillMode::kLetterBoxed:
      width = kExtremeWindowRatioThreshold * height;
      break;
    case OverviewGridWindowFillMode::kPillarBoxed:
      width = height / kExtremeWindowRatioThreshold;
      break;
    default:
      break;
  }

  if (drop_target_ == item) {
    return width;
  }

  // Get the bounds of the item if there is a snapped window or a window
  // about to be snapped. If the height is less than that of the header, there
  // is nothing from the original window to be shown and nothing to be clipped.
  absl::optional<gfx::RectF> split_view_bounds =
      GetSplitviewBoundsMaintainingAspectRatio();
  if (!split_view_bounds || split_view_bounds->height() < kHeaderHeightDp) {
    item->set_unclipped_size(absl::nullopt);
    return width;
  }

  // Perform horizontal clipping if the window's aspect ratio is wider than the
  // split view bounds aspect ratio, and vertical clipping otherwise.
  const float aspect_ratio =
      target_size.width() /
      (target_size.height() -
       item->GetWindow()->GetProperty(aura::client::kTopViewInset));
  const float target_aspect_ratio =
      split_view_bounds->width() / split_view_bounds->height();
  const bool clip_horizontally = aspect_ratio > target_aspect_ratio;
  const int window_height = height - kHeaderHeightDp;
  gfx::Size unclipped_size;
  if (clip_horizontally) {
    unclipped_size.set_width(width);
    unclipped_size.set_height(height);
    // For horizontal clipping, shrink |width| so that the aspect ratio matches
    // that of |split_view_bounds|.
    width = std::max(1, base::ClampFloor(target_aspect_ratio * window_height));
  } else {
    // For vertical clipping, we want |height| to stay the same, so calculate
    // what the unclipped height would be based on |split_view_bounds|.

    // Find the width so that it matches height and matches the aspect ratio of
    // |split_view_bounds|.
    width = split_view_bounds->width() * window_height /
            split_view_bounds->height();
    // The unclipped height is the height which matches |width| but keeps the
    // aspect ratio of |target_bounds|. Clipping takes the overview header into
    // account, so add that back in.
    const int unclipped_height =
        width * target_size.height() / target_size.width();
    unclipped_size.set_width(width);
    unclipped_size.set_height(unclipped_height + kHeaderHeightDp);
  }

  DCHECK(!unclipped_size.IsEmpty());
  item->set_unclipped_size(absl::make_optional(unclipped_size));
  return width;
}

bool OverviewGrid::IsDeskNameBeingModified() const {
  return desks_bar_view_ && desks_bar_view_->IsDeskNameBeingModified();
}

void OverviewGrid::CommitNameChanges() {
  // The desks bar widget may not be ready, since it is created asynchronously
  // later when the entering overview animations finish.
  if (desks_widget_)
    DeskNameView::CommitChanges(desks_widget_.get());

  // The saved desk grid may not be shown.
  if (saved_desk_library_widget_)
    SavedDeskNameView::CommitChanges(saved_desk_library_widget_.get());
}

void OverviewGrid::ShowSavedDeskLibrary() {
  if (!saved_desk_library_widget_) {
    saved_desk_library_widget_ =
        SavedDeskLibraryView::CreateSavedDeskLibraryWidget(root_window_);

    // Compute bounds for the library using the expanded height of the desk
    // bar. `GetGridEffectiveBounds` will not be the correct bounds for the
    // library if we are currently in the zero state mode.
    gfx::Rect library_bounds = bounds_;
    library_bounds.Inset(
        gfx::Insets::TLBR(LegacyDeskBarView::GetPreferredBarHeight(
                              root_window_, LegacyDeskBarView::Type::kOverview,
                              LegacyDeskBarView::State::kExpanded),
                          0, 0, 0));

    saved_desk_library_widget_->SetBounds(library_bounds);
  }

  {
    // Wait until the end to notify content changes for all desks.
    Desk::ScopedContentUpdateNotificationDisabler desks_scoped_notify_disabler(
        /*desks=*/DesksController::Get()->desks(),
        /*notify_when_destroyed=*/true);

    for (auto& overview_mode_item : window_list_) {
      overview_mode_item->HideForSavedDeskLibrary(/*animate=*/true);
    }
  }

  // There may be an existing animation in progress triggered by
  // `HideSavedDeskLibrary()` below, which animates a widget to 0.f before
  // calling `OnSavedDeskGridFadedOut()` to hide the widget on animation end.
  // Stop animating so that the callbacks associated get fired, otherwise we may
  // end up trying to show a widget that's already shown. `StopAnimating()` is a
  // no-op if there is no animation in progress.
  saved_desk_library_widget_->GetLayer()->GetAnimator()->StopAnimating();
  saved_desk_library_widget_->Show();

  // Fade in the widget from its current opacity.
  PerformFadeInLayer(saved_desk_library_widget_->GetLayer(),
                     /*animate=*/true);

  UpdateSaveDeskButtons();

  // If the overview desk bar is not created at this point, create it. This is
  // only possible for clicking library button on the desk button desk bar,
  // since for the overview desk bar, there has to be a bar before we can click
  // on the library button.
  if (!desks_widget_) {
    MaybeInitDesksWidget();
  }

  // When desks bar is at zero state, the library button's state update will be
  // handled by `UpdateNewMiniViews` when expanding the desks bar.
  if (desks_bar_view_->IsZeroState()) {
    desks_bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                        /*expanding_bar_view=*/true);
  } else if (chromeos::features::IsJellyrollEnabled()) {
    desks_bar_view_->UpdateDeskIconButtonState(
        desks_bar_view_->library_button(),
        /*target_state=*/CrOSNextDeskIconButton::State::kActive);
  }

  desks_bar_view_->UpdateButtonsForSavedDeskGrid();
}

void OverviewGrid::HideSavedDeskLibrary(bool exit_overview) {
  if (!saved_desk_library_widget_)
    return;

  auto* grid_layer = saved_desk_library_widget_->GetLayer();
  const bool already_hiding_grid = grid_layer->GetAnimator()->is_animating() &&
                                   grid_layer->GetTargetOpacity() == 0.f;
  if (already_hiding_grid)
    return;

  // Wait until the end to notify content changes for all desks.
  Desk::ScopedContentUpdateNotificationDisabler desks_scoped_notify_disabler(
      /*desks=*/DesksController::Get()->desks(),
      /*notify_when_destroyed=*/true);

  if (exit_overview && overview_session_->enter_exit_overview_type() ==
                           OverviewEnterExitType::kImmediateExit) {
    // Since we're immediately exiting, we don't need to animate anything.
    // Reshow the overview items and let the `saved_desk_library_widget_`
    // handle its own destruction.
    for (auto& overview_mode_item : window_list_)
      overview_mode_item->RevertHideForSavedDeskLibrary(/*animate=*/false);
    return;
  }

  if (exit_overview) {
    // Un-hide the overview mode items.
    for (auto& overview_mode_item : window_list_)
      overview_mode_item->RevertHideForSavedDeskLibrary(/*animate=*/true);

    // Disable the `saved_desk_library_widget_`'s event targeting so it can't
    // get any events during the animation.
    saved_desk_library_widget_->GetNativeWindow()->SetEventTargetingPolicy(
        aura::EventTargetingPolicy::kNone);

    FadeOutWidgetFromOverview(
        std::move(saved_desk_library_widget_),
        OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_SAVED_DESK_GRID_FADE_OUT);
    return;
  }

  // Fade out the `saved_desk_library_widget_` and then when its animation is
  // done fade in the supporting widgets and revert the overview item hides.
  PerformFadeOutLayer(saved_desk_library_widget_->GetLayer(),
                      /*animate=*/true,
                      base::BindOnce(&OverviewGrid::OnSavedDeskGridFadedOut,
                                     weak_ptr_factory_.GetWeakPtr()));
  if (chromeos::features::IsJellyrollEnabled()) {
    // The saved desk library is hidden because of a new desk is created for
    // saved desk. We have animation of adding a new desk for the library
    // button, thus to avoid the animation glitches, directly update the state
    // for the library button instead of applying the scale animation to it.
    desks_bar_view_->library_button()->UpdateState(
        CrOSNextDeskIconButton::State::kExpanded);
  }
}

bool OverviewGrid::IsShowingSavedDeskLibrary() const {
  return saved_desk_library_widget_ &&
         saved_desk_library_widget_->IsVisible() &&
         saved_desk_library_widget_->GetLayer()->GetTargetOpacity() == 1.0f;
}

bool OverviewGrid::IsSavedDeskNameBeingModified() const {
  if (auto* library_view = GetSavedDeskLibraryView()) {
    for (auto* grid_view : library_view->grid_views()) {
      if (grid_view->IsSavedDeskNameBeingModified()) {
        return true;
      }
    }
  }
  return false;
}

void OverviewGrid::UpdateNoWindowsWidget(bool no_items,
                                         bool animate,
                                         bool is_continuous_enter) {
  // If `kFasterSplitScreenSetup` or `kSnapGroup` is enabled, hide the widget if
  // we aren't in split view, otherwise hide the widget if there is an item in
  // overview or the saved desk grid is visible.
  // TODO(b/307812315): Rename to `faster_splitscreen_widget_` if we'll always
  // show this.
  if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled()
          ? !RootWindowController::ForWindow(root_window())
                 ->split_view_overview_session()
          : !no_items || IsShowingSavedDeskLibrary()) {
    no_windows_widget_.reset();
    return;
  }

  if (!no_windows_widget_) {
    // Create and fade in the widget.
    RoundedLabelWidget::InitParams params;
    params.name = "OverviewNoWindowsLabel";
    params.horizontal_padding = kNoItemsIndicatorHorizontalPaddingDp;
    params.vertical_padding = kNoItemsIndicatorVerticalPaddingDp;
    params.rounding_dp = kNoItemsIndicatorRoundingDp;
    params.preferred_height = kNoItemsIndicatorHeightDp;
    params.message = IDS_ASH_OVERVIEW_NO_RECENT_ITEMS;
    if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled()) {
      params.message =
          no_items ? kFasterSplitScreenToastNoWindows : kFasterSplitScreenToast;
    }

    params.parent =
        root_window_->GetChildById(desks_util::GetActiveDeskContainerId());
    params.disable_default_visibility_animation = !animate;

    no_windows_widget_ = std::make_unique<RoundedLabelWidget>();
    no_windows_widget_->Init(std::move(params));

    aura::Window* widget_window = no_windows_widget_->GetNativeWindow();
    widget_window->parent()->StackChildAtBottom(widget_window);

    ScopedOverviewAnimationSettings settings(
        animate && !is_continuous_enter ? OVERVIEW_ANIMATION_NO_RECENTS_FADE
                                        : OVERVIEW_ANIMATION_NONE,
        widget_window);
    // Start the opacity at zero for continuous enter. Its opacity will change
    // with the trackpad events as they come.
    no_windows_widget_->SetOpacity(is_continuous_enter ? 0.f : 1.f);
  }

  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewGrid::RefreshNoWindowsWidgetBounds(bool animate) {
  if (!no_windows_widget_)
    return;

  const gfx::Rect grid_bounds(GetGridEffectiveBounds());
  no_windows_widget_->SetBoundsCenteredIn(grid_bounds, animate);

  if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled()) {
    // If there are no windows, set it in the center of the grid.
    if (window_list_.empty()) {
      return;
    }
    // Position the widget under the bottom of the last overview item, but
    // centered horizontally.
    const gfx::RectF last_overview_item_bounds =
        window_list_.back()->target_bounds();
    const int center_x =
        no_windows_widget_->GetBoundsCenteredIn(grid_bounds).x();
    const gfx::Point available_origin(
        center_x,
        last_overview_item_bounds.bottom() + kFasterSplitScreenToastSpacingDp);
    no_windows_widget_->SetBounds(
        gfx::Rect(available_origin,
                  no_windows_widget_->GetContentsView()->GetPreferredSize()));
  }
}

void OverviewGrid::RefreshGridBounds(bool animate) {
  SetBoundsAndUpdatePositions(GetGridBoundsInScreen(root_window_),
                              /*ignored_items=*/{}, animate);
  UpdateNoWindowsWidget(empty(), animate, /*is_continuous_enter=*/false);
}

void OverviewGrid::UpdateSaveDeskButtons() {
  // TODO(crbug.com/1275282): The button should be updated whenever the
  // overview grid changes, i.e. switches between active desks and/or the
  // saved desk grid. This will be needed when we make it so that switching
  // desks keeps us in overview mode.
  if (!saved_desk_util::IsSavedDesksEnabled())
    return;

  // If there is only one item and it is animating to close, hide the widget as
  // the closing window cannot be saved as part of a template.
  const bool no_items =
      window_list_.empty() ||
      (window_list_.size() == 1u && window_list_.front()->animating_to_close());

  // Do not create or show the save desk buttons if there are no
  // windows in this grid, during a window drag or in tablet mode, the saved
  // desk grid is visible, if the desks bar hasn't been created yet, or if the
  // feature ContinuousOverviewScrollAnimation is enabled and a continuous
  // scroll is in progress.
  const bool target_visible =
      !no_items && !overview_session_->GetCurrentDraggedOverviewItem() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !IsShowingSavedDeskLibrary() && desks_widget_ &&
      (!features::IsContinuousOverviewScrollAnimationEnabled() ||
       !OverviewController::Get()->is_continuous_scroll_in_progress());

  const bool visibility_changed =
      target_visible != IsSaveDeskButtonContainerVisible();

  // Adds or removes the widget from the accessibility focus order when exiting
  // the scope. Skip the update if the widget's visibility hasn't changed.
  base::ScopedClosureRunner update_accessibility_focus(base::BindOnce(
      [](OverviewSession* session, bool widget_visibility_changed) {
        if (widget_visibility_changed)
          session->UpdateAccessibilityFocus();
      },
      overview_session_, visibility_changed));

  if (!target_visible) {
    if (visibility_changed && save_desk_button_container_widget_) {
      PerformFadeOutLayer(
          save_desk_button_container_widget_->GetLayer(),
          /*animate=*/true,
          base::BindOnce(&OverviewGrid::OnSaveDeskButtonContainerFadedOut,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  // Create `save_desk_button_container_widget_`.
  if (!save_desk_button_container_widget_) {
    save_desk_button_container_widget_ =
        CreateSaveDeskButtonContainerWidget(root_window_);
    save_desk_button_container_widget_->SetContentsView(
        std::make_unique<SavedDeskSaveDeskButtonContainer>(
            base::BindRepeating(
                &OverviewGrid::OnSaveDeskAsTemplateButtonPressed,
                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&OverviewGrid::OnSaveDeskForLaterButtonPressed,
                                weak_ptr_factory_.GetWeakPtr())));
  }

  // If a desk animation is in progress, we don't want to animate
  // `save_desk_button_container_widget_`.
  const bool in_desk_animation = DesksController::Get()->animation();

  // There may be an existing animation in progress triggered by
  // `PerformFadeOutLayer()` above, which animates a widget to 0.f before
  // calling `OnSaveDeskButtonContainerFadedOut()` to hide the widget on
  // animation end. Stop animating so that the callbacks associated get fired,
  // otherwise we may end up trying to show a widget that's already shown.
  // `StopAnimating()` is a no-op if there is no animation in progress.
  if (visibility_changed) {
    save_desk_button_container_widget_->GetLayer()
        ->GetAnimator()
        ->StopAnimating();
    save_desk_button_container_widget_->Show();
    PerformFadeInLayer(save_desk_button_container_widget_->GetLayer(),
                       /*animate=*/!in_desk_animation);
  }

  auto* split_view_controller = SplitViewController::Get(root_window_);
  int snapped_unsupported_window = 0;
  int snapped_incognito_window = 0;
  int snapped_supported_window = 0;
  if (split_view_controller->InSplitViewMode()) {
    aura::Window* window = split_view_controller->GetDefaultSnappedWindow();
    if (IsUnsupportedWindow(window)) {
      snapped_unsupported_window = 1;
    } else if (IsIncognitoWindow(window)) {
      snapped_incognito_window = 1;
    } else {
      snapped_supported_window = 1;
    }
  }

  // Enable/disable button and update tooltip.
  const SavedDeskPresenter* saved_desk_presenter =
      overview_session_->saved_desk_presenter();
  SavedDeskSaveDeskButtonContainer* container =
      static_cast<SavedDeskSaveDeskButtonContainer*>(
          save_desk_button_container_widget_->GetContentsView());
  container->UpdateButtonEnableStateAndTooltip(
      SavedDeskSaveDeskButton::Type::kSaveAsTemplate,
      saved_desk_presenter->GetEntryCount(DeskTemplateType::kTemplate),
      saved_desk_presenter->GetMaxEntryCount(DeskTemplateType::kTemplate),
      num_incognito_windows_ + snapped_incognito_window,
      num_unsupported_windows_ + snapped_unsupported_window,
      size() + snapped_incognito_window + snapped_unsupported_window +
          snapped_supported_window);
  container->UpdateButtonEnableStateAndTooltip(
      SavedDeskSaveDeskButton::Type::kSaveForLater,
      saved_desk_presenter->GetEntryCount(DeskTemplateType::kSaveAndRecall),
      saved_desk_presenter->GetMaxEntryCount(DeskTemplateType::kSaveAndRecall),
      num_incognito_windows_ + snapped_incognito_window,
      num_unsupported_windows_ + snapped_unsupported_window,
      size() + snapped_incognito_window + snapped_unsupported_window +
          snapped_supported_window);

  // Set the widget position above the overview item window and default width
  // and height.
  gfx::RectF first_overview_item_bounds;
  if (window_list_.front()->animating_to_close()) {
    DCHECK_GT(window_list_.size(), 1u);
    first_overview_item_bounds = window_list_[1]->target_bounds();
  } else {
    first_overview_item_bounds = window_list_.front()->target_bounds();
  }

  // Animate the widget so it moves with the items. The widget's size isn't
  // changing, so its ok to use a bounds animation as opposed to a transform
  // animation. If the visibility has changed, skip the bounds animation and use
  // the fade animation from above. Align the widget so it is visually aligned
  // with the first overview item.
  ScopedOverviewAnimationSettings settings(
      visibility_changed || in_desk_animation
          ? OVERVIEW_ANIMATION_NONE
          : OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW,
      save_desk_button_container_widget_->GetNativeWindow());
  gfx::Point available_origin =
      gfx::ToRoundedPoint(first_overview_item_bounds.origin()) +
      gfx::Vector2d(0, -kSaveDeskAsTemplateOverviewItemSpacingDp);
  save_desk_button_container_widget_->SetBounds(gfx::Rect(
      available_origin, save_desk_button_container_widget_->GetContentsView()
                            ->GetPreferredSize()));
}

void OverviewGrid::EnableSaveDeskButtonContainer() {
  if (!save_desk_button_container_widget_ ||
      overview_session_->is_shutting_down()) {
    return;
  }
  save_desk_button_container_widget_->GetContentsView()->SetEnabled(true);
}

bool OverviewGrid::IsSaveDeskButtonContainerVisible() const {
  // The widget may be visible but in the process of fading away. We treat that
  // as not visible.
  return save_desk_button_container_widget_ &&
         save_desk_button_container_widget_->IsVisible() &&
         save_desk_button_container_widget_->GetLayer()->GetTargetOpacity() ==
             1.f;
}

bool OverviewGrid::IsSaveDeskAsTemplateButtonVisible() const {
  if (!IsSaveDeskButtonContainerVisible())
    return false;
  SavedDeskSaveDeskButtonContainer* container =
      static_cast<SavedDeskSaveDeskButtonContainer*>(
          save_desk_button_container_widget_->GetContentsView());
  return container->save_desk_as_template_button() &&
         container->save_desk_as_template_button()->GetVisible();
}

bool OverviewGrid::IsSaveDeskForLaterButtonVisible() const {
  if (!IsSaveDeskButtonContainerVisible())
    return false;
  SavedDeskSaveDeskButtonContainer* container =
      static_cast<SavedDeskSaveDeskButtonContainer*>(
          save_desk_button_container_widget_->GetContentsView());
  return container->save_desk_for_later_button() &&
         container->save_desk_for_later_button()->GetVisible();
}

SavedDeskSaveDeskButton* OverviewGrid::GetSaveDeskAsTemplateButton() const {
  return save_desk_button_container_widget_
             ? static_cast<SavedDeskSaveDeskButtonContainer*>(
                   save_desk_button_container_widget_->GetContentsView())
                   ->save_desk_as_template_button()
             : nullptr;
}

SavedDeskSaveDeskButton* OverviewGrid::GetSaveDeskForLaterButton() const {
  return save_desk_button_container_widget_
             ? static_cast<SavedDeskSaveDeskButtonContainer*>(
                   save_desk_button_container_widget_->GetContentsView())
                   ->save_desk_for_later_button()
             : nullptr;
}

SavedDeskSaveDeskButtonContainer* OverviewGrid::GetSaveDeskButtonContainer()
    const {
  return save_desk_button_container_widget_
             ? static_cast<SavedDeskSaveDeskButtonContainer*>(
                   save_desk_button_container_widget_->GetContentsView())
             : nullptr;
}

void OverviewGrid::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Do nothing if overview is being shutdown.
  OverviewController* overview_controller = OverviewController::Get();
  if (!overview_controller->InOverviewSession()) {
    return;
  }

  if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled()) {
    // When an activated window is auto snapped, it will send a state change and
    // try to end overview here. Ignore split view state when
    // `kFasterSplitScreenSetup` or `kSnapGroup` arm1 is enabled.
    return;
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  const bool unsnappable_window_activated =
      state == SplitViewController::State::kNoSnap &&
      split_view_controller->end_reason() ==
          SplitViewController::EndReason::kUnsnappableWindowActivated;

  // If two windows were snapped to both sides of the screen or an unsnappable
  // window was just activated, or we're in single split mode in clamshell mode
  // and there is no window in overview, end overview mode and bail out.
  if (state == SplitViewController::State::kBothSnapped ||
      unsnappable_window_activated ||
      (split_view_controller->InClamshellSplitViewMode() &&
       overview_session_->IsEmpty())) {
    overview_session_->RestoreWindowActivation(false);
    overview_controller->EndOverview(OverviewEndAction::kSplitView);
    return;
  }

  // Update the cannot snap warnings and adjust the grid bounds.
  UpdateCannotSnapWarningVisibility(/*animate=*/true);
  RefreshGridBounds(/*animate=*/false);

  // If split view mode was ended, then activate the overview focus window, to
  // match the behavior of entering overview mode in the beginning.
  if (state == SplitViewController::State::kNoSnap)
    wm::ActivateWindow(overview_session_->GetOverviewFocusWindow());
}

void OverviewGrid::OnSplitViewDividerPositionChanged() {
  if (window_util::IsFasterSplitScreenOrSnapGroupArm1Enabled()) {
    // If `IsFasterSplitScreenOrSnapGroupArm1Enabled` is true,
    // `SplitViewOverviewSession` will manually update the bounds so we don't
    // need to update here.
    return;
  }

  SetBoundsAndUpdatePositions(
      GetGridBoundsInScreen(root_window_,
                            /*window_dragging_state=*/absl::nullopt,
                            /*divider_changed=*/true,
                            /*account_for_hotseat=*/true),
      /*ignored_items=*/{}, /*animate=*/false);
}

void OverviewGrid::OnScreenCopiedBeforeRotation() {
  OverviewController::Get()->PauseOcclusionTracker();

  for (auto& window : window_list()) {
    window->UpdateRoundedCornersAndShadow();
    window->StopWidgetAnimation();
  }
}

void OverviewGrid::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {
  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->DelayedUpdateRoundedCornersAndShadow();
  overview_controller->UnpauseOcclusionTracker(
      kOcclusionUnpauseDurationForRotation);
}

void OverviewGrid::OnWallpaperChanging() {
  grid_event_handler_.reset();
}

void OverviewGrid::OnWallpaperChanged() {
  grid_event_handler_ = std::make_unique<OverviewGridEventHandler>(this);
}

void OverviewGrid::OnOverviewItemWindowDestroying(OverviewItem* overview_item,
                                                  bool reposition) {
  // `this` will be the delegate to handle the window destroying if the
  // underlying window represented by the corresponding overview item is a
  // single window.
  // Remove the item from `overview_session_` which will remove it from the
  // grid. If `overview_session_` is not available then remove it from the grid
  // directly.
  // TODO(b/299391958): Investigate why `overview_session_` might be unavailable
  // while grid is still alive.
  if (overview_session_) {
    overview_session_->RemoveItem(overview_item, /*item_destroying=*/true,
                                  reposition);
  } else {
    RemoveItem(overview_item, /*item_destroying=*/true, reposition);
  }
}

SavedDeskLibraryView* OverviewGrid::GetSavedDeskLibraryView() const {
  return saved_desk_library_widget_
             ? static_cast<SavedDeskLibraryView*>(
                   saved_desk_library_widget_->GetContentsView())
             : nullptr;
}

void OverviewGrid::MaybeInitDesksWidget() {
  TRACE_EVENT0("ui", "OverviewGrid::MaybeInitDesksWidget");
  if (!desks_util::ShouldDesksBarBeCreated() || desks_widget_)
    return;

  desks_widget_ = DeskBarViewBase::CreateDeskWidget(
      root_window_, GetDesksWidgetBounds(), DeskBarViewBase::Type::kOverview);

  // The following order of function calls is significant: SetContentsView()
  // must be called before LegacyDeskBarView:: Init(). This is needed because
  // the desks mini views need to access the widget to get the root window in
  // order to know how to layout themselves.
  desks_bar_view_ = desks_widget_->SetContentsView(
      std::make_unique<LegacyDeskBarView>(weak_ptr_factory_.GetWeakPtr()));
  desks_bar_view_->Init();

  // If the feature ContinuousOverviewScrollAnimation is enabled and a
  // continuous scroll is now starting, move the desk bar up so we can slowly
  // place it downward in relation to the scroll offset.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewEnterExitType::kContinuousAnimationEnterOnScrollUpdate) {
    gfx::Transform transform;
    transform.Translate(0, -desks_bar_view_->GetBoundsInScreen().height());
    auto* layer = desks_bar_view_->layer();
    layer->SetTransform(transform);
  }

  desks_widget_->Show();

  // TODO(afakhry): Check if we need to keep this as the bottom-most window in
  // the container.
  auto* window = desks_widget_->GetNativeWindow();
  window->parent()->StackChildAtBottom(window);
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRects(
    const base::flat_set<OverviewItemBase*>& ignored_items) {
  gfx::Rect total_bounds = GetGridEffectiveBounds();

  // Windows occupy vertically centered area with additional vertical insets.
  total_bounds.Inset(GetGridInsetsImpl(total_bounds));
  std::vector<gfx::RectF> rects;

  // Keep track of the lowest coordinate.
  int max_bottom = total_bounds.y();

  // Right bound of the narrowest row.
  int min_right = total_bounds.right();
  // Right bound of the widest row.
  int max_right = total_bounds.x();

  // Keep track of the difference between the narrowest and the widest row.
  // Initially this is set to the worst it can ever be assuming the windows fit.
  int width_diff = total_bounds.width();

  // Initially allow the windows to occupy all available width. Shrink this
  // available space horizontally to find the breakdown into rows that achieves
  // the minimal |width_diff|.
  int right_bound = total_bounds.right();

  // Determine the optimal height bisecting between |low_height| and
  // |high_height|. Once this optimal height is known, |height_fixed| is set to
  // true and the rows are balanced by repeatedly squeezing the widest row to
  // cause windows to overflow to the subsequent rows.
  int low_height = kVerticalSpaceBetweenItemsDp;
  int high_height = std::max(low_height, total_bounds.height() + 1);
  int height = 0.5 * (low_height + high_height);
  bool height_fixed = false;

  // Repeatedly try to fit the windows |rects| within |right_bound|.
  // If a maximum |height| is found such that all window |rects| fit, this
  // fitting continues while shrinking the |right_bound| in order to balance the
  // rows. If the windows fit the |right_bound| would have been decremented at
  // least once so it needs to be incremented once before getting out of this
  // loop and one additional pass made to actually fit the |rects|.
  // If the |rects| cannot fit (e.g. there are too many windows) the bisection
  // will still finish and we might increment the |right_bound| once pixel extra
  // which is acceptable since there is an unused margin on the right.
  bool make_last_adjustment = false;
  while (true) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    bool windows_fit = FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight, height), ignored_items,
        &rects, &max_bottom, &min_right, &max_right);

    if (height_fixed) {
      if (!windows_fit) {
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
      // Break if all the windows are zero-width at the current scale.
      if (max_right <= total_bounds.x())
        break;
    } else {
      // Find the optimal row height bisecting between |low_height| and
      // |high_height|.
      if (windows_fit)
        low_height = height;
      else
        high_height = height;
      height = 0.5 * (low_height + high_height);
      // When height can no longer be improved, start balancing the rows.
      if (height == low_height)
        height_fixed = true;
    }

    if (windows_fit && height_fixed) {
      if (max_right - min_right <= width_diff) {
        // Row alignment is getting better. Try to shrink the |right_bound| in
        // order to squeeze the widest row.
        right_bound = max_right - 1;
        width_diff = max_right - min_right;
      } else {
        // Row alignment is getting worse.
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
    }
  }
  // Once the windows in |window_list_| no longer fit, the change to
  // |right_bound| was reverted. Perform one last pass to position the |rects|.
  if (make_last_adjustment) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    FitWindowRectsInBounds(overview_mode_bounds, std::min(kMaxHeight, height),
                           ignored_items, &rects, &max_bottom, &min_right,
                           &max_right);
  }

  gfx::Vector2dF offset(0, (total_bounds.bottom() - max_bottom) / 2.f);
  for (auto& rect : rects)
    rect += offset;
  return rects;
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRectsForScrollingLayout(
    const base::flat_set<OverviewItemBase*>& ignored_items) {
  gfx::Rect total_bounds = GetGridEffectiveBounds();
  // Windows occupy vertically centered area with additional vertical insets.
  total_bounds.Inset(GetGridInsetsImpl(total_bounds));
  total_bounds.Inset(
      gfx::Insets::TLBR(kTabletModeOverviewItemTopPaddingDp, 0, 0, 0));

  // `scroll_offset_min_` may be changed on positioning (either by closing
  // windows or display changes). Recalculate it and clamp `scroll_offset_`, so
  // that the items are always aligned left or right.
  float rightmost_window_right = 0;
  for (const auto& item : window_list_) {
    if (ShouldExcludeItemFromGridLayout(item.get(), ignored_items))
      continue;
    rightmost_window_right =
        std::max(rightmost_window_right, item->target_bounds().right());
  }

  // `rightmost_window_right` may have been modified by an earlier scroll.
  // `scroll_offset_` is added to adjust for that. If `rightmost_window_right`
  // is less than `total_bounds.right()`, the grid cannot be scrolled. Set
  // `scroll_offset_min_` to 0 so that `std::clamp()` is happy.
  rightmost_window_right -= scroll_offset_;
  scroll_offset_min_ = total_bounds.right() - rightmost_window_right;
  if (scroll_offset_min_ > 0.f)
    scroll_offset_min_ = 0.f;

  scroll_offset_ = std::clamp(scroll_offset_, scroll_offset_min_, 0.f);

  // Map which contains up to |kScrollingLayoutRow| entries with information on
  // the last items right bound per row. Used so we can place the next item
  // directly next to the last item. The key is the y-value of the row, and the
  // value is the rightmost x-value.
  base::flat_map<float, float> right_edge_map;

  // Since the number of rows is limited, windows are laid out column-wise so
  // that the most recently used windows are displayed first. When the dragged
  // item becomes an |ignored_item|, move the other windows accordingly.
  // |window_position| matches the positions of the windows' indexes from
  // |window_list_|. However, if a window turns out to be an ignored item,
  // |window_position| remains where the item was as to then reposition the
  // other window's bounds in place of that item.
  const int height = (total_bounds.height() - ((kScrollingLayoutRow - 1) *
                                               kVerticalSpaceBetweenItemsDp)) /
                     kScrollingLayoutRow;
  int window_position = 0;
  std::vector<gfx::RectF> rects;
  for (const auto& window : window_list_) {
    OverviewItemBase* item = window.get();
    if (ShouldExcludeItemFromGridLayout(item, ignored_items)) {
      rects.emplace_back();
      continue;
    }

    // Calculate the width and y position of the item.
    const int width = CalculateWidthAndMaybeSetUnclippedBounds(item, height);
    const int y = (height + kVerticalSpaceBetweenItemsDp) *
                      (window_position % kScrollingLayoutRow) +
                  total_bounds.y();

    // Use the right bounds of the item next to in the row as the x position, if
    // that item exists.
    const int x = right_edge_map.contains(y)
                      ? right_edge_map[y]
                      : total_bounds.x() + scroll_offset_;
    right_edge_map[y] = x + width + kHorizontalSpaceBetweenItemsDp;
    DCHECK_LE(static_cast<int>(right_edge_map.size()), kScrollingLayoutRow);

    const gfx::RectF bounds(x, y, width, height);
    rects.push_back(bounds);
    ++window_position;
  }

  return rects;
}

bool OverviewGrid::FitWindowRectsInBounds(
    const gfx::Rect& bounds,
    int height,
    const base::flat_set<OverviewItemBase*>& ignored_items,
    std::vector<gfx::RectF>* out_rects,
    int* out_max_bottom,
    int* out_min_right,
    int* out_max_right) {
  const size_t window_count = window_list_.size();
  out_rects->resize(window_count);

  // Start in the top-left corner of |bounds|.
  int left = bounds.x();
  int top = bounds.y();

  // Keep track of the lowest coordinate.
  *out_max_bottom = bounds.y();

  // Right bound of the narrowest row.
  *out_min_right = bounds.right();
  // Right bound of the widest row.
  *out_max_right = bounds.x();

  // All elements are of same height and only the height is necessary to
  // determine each item's scale.
  for (size_t i = 0u; i < window_count; ++i) {
    const auto& item = window_list_[i];
    if (ShouldExcludeItemFromGridLayout(item.get(), ignored_items)) {
      continue;
    }

    int width = CalculateWidthAndMaybeSetUnclippedBounds(item.get(), height);

    if ((left + width + kHorizontalSpaceBetweenItemsDp) > bounds.right()) {
      // Move to the next row if possible.
      if (*out_min_right > left)
        *out_min_right = left;
      if (*out_max_right < left)
        *out_max_right = left;
      top += (height + kVerticalSpaceBetweenItemsDp);

      // Check if the new row reaches the bottom or if the first item in the new
      // row does not fit within the available width.
      if ((top + height + kVerticalSpaceBetweenItemsDp) > bounds.bottom() ||
          bounds.x() + width + kHorizontalSpaceBetweenItemsDp >
              bounds.right()) {
        return false;
      }
      left = bounds.x();
    }

    // Position the current rect.
    (*out_rects)[i] = gfx::RectF(left, top, width, height);

    // Increment horizontal position using sanitized positive `width`.
    left += (width + kHorizontalSpaceBetweenItemsDp);

    *out_max_bottom = top + height;
  }

  // Update the narrowest and widest row width for the last row.
  if (*out_min_right > left)
    *out_min_right = left;
  if (*out_max_right < left)
    *out_max_right = left;

  return true;
}

size_t OverviewGrid::GetOverviewItemIndex(OverviewItemBase* item) const {
  auto iter = base::ranges::find(window_list_, item,
                                 &std::unique_ptr<OverviewItemBase>::get);
  DCHECK(iter != window_list_.end());
  return iter - window_list_.begin();
}

size_t OverviewGrid::FindInsertionIndex(const aura::Window* window) {
  const auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  // As we iterate over the whole MRU window list, the windows in this grid
  // will be encountered in the same order, but possibly with other windows in
  // between. Ignore those other windows, and only increment `grid_item_index`
  // when we reach the next window in this grid.
  size_t grid_item_index = 0, mru_window_index = 0;
  while (grid_item_index < size() && mru_window_index < mru_windows.size()) {
    OverviewItemBase* grid_item = window_list_[grid_item_index].get();
    aura::Window* mru_window = mru_windows[mru_window_index];
    if (grid_item == drop_target_ || mru_window == window) {
      return grid_item_index;
    }

    if (grid_item->Contains(mru_window)) {
      grid_item_index++;
    }

    mru_window_index++;
  }

  // If there is no drop target window and `window` is not in the MRU window
  // list, insert at the end.
  return size();
}

void OverviewGrid::AddDraggedWindowIntoOverviewOnDragEnd(
    aura::Window* dragged_window) {
  DCHECK(overview_session_);
  if (overview_session_->IsWindowInOverview(dragged_window))
    return;

  overview_session_->AddItemInMruOrder(dragged_window, /*reposition=*/false,
                                       /*animate=*/false, /*restack=*/true,
                                       /*use_spawn_animation=*/false);
}

gfx::Rect OverviewGrid::GetDesksWidgetBounds() const {
  gfx::Rect desks_widget_screen_bounds = bounds_;
  desks_widget_screen_bounds.set_height(GetDesksBarHeight());

  // Shift the widget down to make room for the splitview indicator guidance
  // when it's shown at the top of the screen and no other windows are snapped.
  if (split_view_drag_indicators_ &&
      split_view_drag_indicators_->current_window_dragging_state() ==
          SplitViewDragIndicators::WindowDraggingState::kFromOverview &&
      !SplitViewController::IsLayoutHorizontal(root_window_) &&
      !SplitViewController::Get(root_window_)->InSplitViewMode()) {
    desks_widget_screen_bounds.Offset(
        0, split_view_drag_indicators_->GetLeftHighlightViewBounds().height() +
               2 * kHighlightScreenEdgePaddingDp);
  }

  return screen_util::SnapBoundsToDisplayEdge(desks_widget_screen_bounds,
                                              root_window_);
}

void OverviewGrid::UpdateCannotSnapWarningVisibility(bool animate) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->UpdateCannotSnapWarningVisibility(animate);
}

void OverviewGrid::OnSaveDeskAsTemplateButtonPressed() {
  auto* container = save_desk_button_container_widget_->GetContentsView();
  // Disable the save desk button container after the first click to prevent
  // unwanted clicks/user interaction.
  if (!container->GetEnabled())
    return;
  container->SetEnabled(false);

  overview_session_->saved_desk_presenter()->MaybeSaveActiveDeskAsSavedDesk(
      DeskTemplateType::kTemplate, root_window());
}

void OverviewGrid::OnSaveDeskForLaterButtonPressed() {
  auto* container = save_desk_button_container_widget_->GetContentsView();
  // Disable the save desk button container after the first click to prevent
  // unwanted clicks/user interaction.
  if (!container->GetEnabled())
    return;
  container->SetEnabled(false);

  overview_session_->saved_desk_presenter()->MaybeSaveActiveDeskAsSavedDesk(
      DeskTemplateType::kSaveAndRecall, root_window());
}

void OverviewGrid::OnSavedDeskGridFadedOut() {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->RevertHideForSavedDeskLibrary(/*animate=*/true);

  saved_desk_library_widget_->Hide();

  desks_bar_view_->UpdateButtonsForSavedDeskGrid();
  desks_bar_view_->OnSavedDeskLibraryHidden();
  UpdateSaveDeskButtons();
  UpdateNoWindowsWidget(/*no_items=*/empty(), /*animate=*/true,
                        /*is_continuous_enter=*/false);
}

void OverviewGrid::OnSaveDeskButtonContainerFadedOut() {
  save_desk_button_container_widget_->Hide();
}

void OverviewGrid::UpdateNumSavedDeskUnsupportedWindows(
    const std::vector<aura::Window*>& windows,
    bool increment) {
  if (!saved_desk_util::IsSavedDesksEnabled())
    return;

  int addend = increment ? 1 : -1;

  // Track the number of unsupported and incognito windows. The saved desk
  // buttons are disabled if there are no supported or non-incognito windows.
  for (auto* window : windows) {
    if (IsUnsupportedWindow(window)) {
      num_unsupported_windows_ += addend;
    } else if (IsIncognitoWindow(window)) {
      num_incognito_windows_ += addend;
    }
  }

  CHECK_GE(num_unsupported_windows_, 0);
  CHECK_GE(num_incognito_windows_, 0);
}

int OverviewGrid::GetDesksBarHeight() const {
  DeskBarViewBase::State state = desks_bar_view_
                                     ? desks_bar_view_->state()
                                     : LegacyDeskBarView::GetPerferredState(
                                           LegacyDeskBarView::Type::kOverview);
  return LegacyDeskBarView::GetPreferredBarHeight(
      root_window_, LegacyDeskBarView::Type::kOverview, state);
}

bool OverviewGrid::ShouldUseScrollingLayout(size_t ignored_items_size) const {
  if (Shell::Get()->IsInTabletMode()) {
    return window_list_.size() - ignored_items_size >=
           kMinimumItemsForScrollingLayout;
  }

  return false;
}

void OverviewGrid::AddDropTargetImpl(OverviewItemBase* dragged_item,
                                     size_t position,
                                     bool animate) {
  CHECK(!drop_target_);

  auto drop_target = std::make_unique<OverviewDropTarget>(this);
  drop_target_ = drop_target.get();
  window_list_.insert(window_list_.begin() + position, std::move(drop_target));

  const size_t num_items = overview_session_->num_items();
  overview_session_->set_num_items(num_items + 1u);

  base::flat_set<OverviewItemBase*> ignored_items;
  if (dragged_item) {
    ignored_items.insert(dragged_item);
  }
  PositionWindows(animate, ignored_items);
}

}  // namespace ash
