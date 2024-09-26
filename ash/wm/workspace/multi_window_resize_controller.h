// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/mouse_watcher.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ash {

class MultiWindowResizeControllerTest;
class WorkspaceWindowResizer;

// MultiWindowResizeController is responsible for determining and showing a
// widget that allows resizing multiple windows at the same time.
// MultiWindowResizeController is driven by WorkspaceEventHandler. It can also
// be an entry point to create or remove a snap group which is guarded by the
// feature flag `kSnapGroup` and will only be available when the feature param
// `kAutomaticallyLockGroup` is false.
class ASH_EXPORT MultiWindowResizeController
    : public views::MouseWatcherListener,
      public aura::WindowObserver,
      public WindowStateObserver,
      public OverviewObserver {
 public:
  // Delay before showing the `resize_widget_`.
  static constexpr base::TimeDelta kShowDelay = base::Milliseconds(400);

  MultiWindowResizeController();

  MultiWindowResizeController(const MultiWindowResizeController&) = delete;
  MultiWindowResizeController& operator=(const MultiWindowResizeController&) =
      delete;

  ~MultiWindowResizeController() override;

  // If necessary, shows the resize widget. |window| is the window the mouse
  // is over, |component| the edge and |point| the location of the mouse.
  void Show(aura::Window* window, int component, const gfx::Point& point);

  // MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  IconButton* lock_button_for_testing() const { return lock_button_; }

 private:
  friend class MultiWindowResizeControllerTest;
  friend class SnapGroupTest;
  class ResizeMouseWatcherHost;
  class ResizeView;

  // Two directions resizes happen in.
  enum class Direction {
    kTopBottom,
    kLeftRight,
  };

  // Used to track the two resizable windows and direction.
  struct ResizeWindows {
    ResizeWindows();
    ResizeWindows(const ResizeWindows& other);
    ~ResizeWindows();

    // Returns true if |other| equals this ResizeWindows. This does *not*
    // consider the windows in |other_windows|.
    bool Equals(const ResizeWindows& other) const;

    // Returns true if this ResizeWindows is valid.
    bool is_valid() const { return window1 && window2; }

    // The left/top window to resize.
    raw_ptr<aura::Window, ExperimentalAsh> window1 = nullptr;

    // Other window to resize.
    raw_ptr<aura::Window, ExperimentalAsh> window2 = nullptr;

    // Direction
    Direction direction;

    // Windows after |window2| that are to be resized. Determined at the time
    // the resize starts.
    std::vector<aura::Window*> other_windows;
  };

  void CreateMouseWatcher();

  // Returns a ResizeWindows based on the specified arguments. Use is_valid()
  // to test if the return value is a valid multi window resize location.
  ResizeWindows DetermineWindows(aura::Window* window,
                                 int window_component,
                                 const gfx::Point& point) const;

  // Variant of DetermineWindows() that uses the current location of the mouse
  // to determine the resize windows.
  ResizeWindows DetermineWindowsFromScreenPoint(aura::Window* window) const;

  // Finds a window by edge (one of the constants HitTestCompat.
  aura::Window* FindWindowByEdge(aura::Window* window_to_ignore,
                                 int edge_want,
                                 int x_in_parent,
                                 int y_in_parent) const;

  // Returns the first window touching `window`.
  aura::Window* FindWindowTouching(aura::Window* window,
                                   Direction direction) const;

  // Places any windows touching `start` into `others`.
  void FindWindowsTouching(aura::Window* start,
                           Direction direction,
                           std::vector<aura::Window*>* others) const;

  // Starts/Stops observing `window`.
  void StartObserving(aura::Window* window);
  void StopObserving(aura::Window* window);

  // Check if we're observing `window`.
  bool IsObserving(aura::Window* window) const;

  // Shows the resizer if the mouse is still at a valid location. This is called
  // from the `show_timer_`.
  void ShowIfValidMouseLocation();

  // Shows the `resize_widget_` and `lock_widget_` immediately.
  void ShowNow();

  // Returns true if the `resize_widget_` and `lock_widget_` are showing.
  bool IsShowing() const;

  // Hides the `resize_widget_` and `lock_widget_` if they get created.
  void Hide();

  // Resets the window resizer and hides the widgets.
  void ResetResizer();

  // Initiates a resize.
  void StartResize(const gfx::PointF& location_in_screen);

  // Resizes to the new location.
  void Resize(const gfx::PointF& location_in_screen, int event_flags);

  // Completes the resize.
  void CompleteResize();

  // Cancels the resize.
  void CancelResize();

  // Returns the bounds for the resize widget.
  gfx::Rect CalculateResizeWidgetBounds(
      const gfx::PointF& location_in_parent) const;

  // Returns the bounds for the `lock_widget_` based on the
  // `resize_widget_bounds`.
  gfx::Rect CalculateLockWidgetBounds(
      const gfx::Rect& resize_widget_bounds) const;

  // Returns true if `location_in_screen` is over the resize widget.
  bool IsOverResizeWidget(const gfx::Point& location_in_screen) const;

  // Returns true if `location_in_screen` is over the resize widget.
  // TODO(michelefan): combine with `IsOverResizeWidget` to create a more
  // general function if arm2 under the `kSnapGroup` flag is enabled by default.
  bool IsOverLockWidget(const gfx::Point& location_in_screen) const;

  // Returns true if `location_in_screen` is over the resize windows
  // (or the resize widget itself).
  bool IsOverWindows(const gfx::Point& location_in_screen) const;

  // Returns true if |location_in_screen| is over |component| in |window|.
  bool IsOverComponent(aura::Window* window,
                       const gfx::Point& location_in_screen,
                       int component) const;

  // Called when the lock button is pressed to create a snap group.
  void OnLockButtonPressed();

  // Returns true if a lock wiget should be considered i.e. when two windows are
  // both snapped and the feature flag `kSnapGroup` is enabled.
  bool ShouldConsiderLockWidget() const;

  // Windows and direction to resize.
  ResizeWindows windows_;

  // Timer used before showing.
  base::OneShotTimer show_timer_;

  std::unique_ptr<views::Widget> resize_widget_;

  // The lock widget that is used to create or remove a snap group when
  // `kAutomaticallyLockGroup` of `kSnapGroup` is false.
  std::unique_ptr<views::Widget> lock_widget_;

  // The contents view of the `lock_widget_`.
  raw_ptr<IconButton, ExperimentalAsh> lock_button_;

  // If non-null we're in a resize loop.
  std::unique_ptr<WorkspaceWindowResizer> window_resizer_;

  // Mouse coordinate passed to Show() in container's coodinates.
  gfx::Point show_location_in_parent_;

  // Bounds the resize widget was last shown at in screen coordinates.
  gfx::Rect resize_widget_show_bounds_in_screen_;

  // Bounds the lock widget was last shown at in screen coordinates.
  gfx::Rect lock_widget_show_bounds_in_screen_;

  // Used to detect whether the mouse is over the windows. While
  // |resize_widget_| is non-NULL (ie the widget is showing) we ignore calls
  // to Show().
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  base::ScopedMultiSourceObservation<WindowState, WindowStateObserver>
      window_state_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_
