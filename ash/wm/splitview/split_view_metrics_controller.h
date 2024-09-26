// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_

#include <cstdint>
#include <set>
#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  //  namespace aura

namespace ash {
class SplitViewController;

// SplitViewMetricsController:
// Manages split view related metrics. Tablet mode split view and clamshell
// split view with overview next to a snapped window are managed by
// `SplitViewController`. The UMA can be recorded by the corresponding methods
// of `SplitViewObserver`.
//
// There is another clamshell split view mode with two windows snapped on both
// sides which is not managed by the `SplitViewController`. Therefore,
// `SplitViewMetricsController` needs to inspect this type of split view mode
// whose entry and exit points are defined as below:
// Entry: When the top two visible windows on the active desk are snapped on
//        the left and right sides respectively, the clamshell split view
//        starts.
// Pause: After the split view starts, an unsnapped window is activated,
//        covering the two windows snapped on both sides. The clamshell split
//        view is paused (accumulate the engagement time without reporting to
//        the UMA).
// End: When no two windows are snapped on both sides or tablet model split view
//        starts, the clamshell split view ends.
class SplitViewMetricsController : public TabletModeObserver,
                                   public SplitViewObserver,
                                   public display::DisplayObserver,
                                   public aura::WindowObserver,
                                   public WindowStateObserver,
                                   public wm::ActivationChangeObserver,
                                   public DesksController::Observer,
                                   public aura::EnvObserver {
 public:
  // Enumeration of device mode when entering split view.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DeviceUIMode {
    kClamshell,
    kTablet,
    kMaxValue = kTablet,
  };

  // Enumeration of device orientation when entering and using split view.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DeviceOrientation {
    // Left and right.
    kLandscape,
    // Top and bottom.
    kPortrait,
    kMaxValue = kPortrait,
  };

  // static
  static SplitViewMetricsController* Get(aura::Window* window);

  // `SplitViewMetricsController` is attached to a `SplitViewController` with
  // the same root window.
  explicit SplitViewMetricsController(
      SplitViewController* split_view_controller);
  SplitViewMetricsController(const SplitViewMetricsController&) = delete;
  SplitViewMetricsController& operator=(const SplitViewMetricsController&) =
      delete;

  ~SplitViewMetricsController() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewWindowResized() override;
  void OnSplitViewWindowSwapped() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnResizeLoopEnded(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // DesksController::Observer:
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;

  // aura::EnvObserver
  void OnWindowInitialized(aura::Window* window) override;

  bool in_split_view_recording() const { return in_split_view_recording_; }

 private:
  // Calls when start to record split view metrics.
  void StartRecordSplitViewMetrics();
  // Calls when stop recording split view metrics.
  void StopRecordSplitViewMetrics();

  // Checks if the `window` is in the `observed_windows_` list. Returns true, if
  // the window is being observed.
  bool IsObservingWindow(aura::Window* window) const;

  // Adds and removes a window to the `observed_windows_` list. Note that adding
  // (removing) window and window state observers should be performed at the
  // same time with adding (removing) observed windows.
  void AddObservedWindow(aura::Window* window);
  void RemoveObservedWindow(aura::Window* window);

  // Attaches a new window to the end of `observed_windows_` (the window is
  // stacked on top). If the window is already in the list, just stacks it on
  // top.
  void AddOrStackWindowOnTop(aura::Window* window);

  // Adds windows on active desk to `observed_windows_` list.
  void InitObservedWindowsOnActiveDesk();
  // Remove all current observed windows.
  void ClearObservedWindows();

  // If there are top windows snapped on both sides, start to record split
  // view metrics. Otherwise, stop recording split view metrics.
  void MaybeStartOrEndRecordBothSnappedClamshellSplitView();
  // Pauses recording of engagement time when a window hides the windows
  // snapped on both sides. Return true, if the recording is paused. Otherwise,
  // return false.
  bool MaybePauseRecordBothSnappedClamshellSplitView();

  // Resets the variables related to time and counter metrics.
  void ResetTimeAndCounter();

  // Checks if we are recording clamshell/tablet mode metrics.
  bool IsRecordingClamshellMetrics() const;
  bool IsRecordingTabletMetrics() const;

  // Reports the engagement metrics for both clamshell and tablet split view.
  void StartRecordClamshellSplitView();
  void StopRecordClamshellSplitView();
  void StartRecordTabletSplitView();
  void StopRecordTabletSplitView();

  // Reports the engagement metrics for both multi-display clamshell and tablet
  // split view. Note that the multi-display mode is managed by the split view
  // metrics controllers of all root windows:
  // - After a root window enters split view, the total number of root windows
  //   in split view becomes two, indicating that multi-display split view
  //   started.
  // - After a root window exits split view, the total number of root windows in
  //   split view becomes one, indicating that multi-display split view ended.
  // - In any time, the total number of root windows in split view larger than
  //   one indicating that it is in the multi-display split view.
  void StartRecordClamshellMultiDisplaySplitView();
  void StopRecordClamshellMultiDisplaySplitView();
  void StartRecordTabletMultiDisplaySplitView();
  void StopRecordTabletMultiDisplaySplitView();

  // Called when the display orientation or mode changes to report device mode
  // and orientation the user uses split screen in. This updates UMA metric
  // `Ash.SplitView.DeviceOrientation.{DeviceUIMode}`.
  void ReportDeviceUIModeAndOrientationHistogram();

  // We need to save an ptr of the observed `SplitViewController`. Because the
  // `RootWindowController` will be deconstructed in advance. Then, we cannot
  // use it to get observed `SplitViewController`.
  const raw_ptr<SplitViewController, ExperimentalAsh> split_view_controller_;

  // Indicates whether it is recording split view metrics.
  bool in_split_view_recording_ = false;

  // Used to track the change of device orientation.
  DeviceOrientation orientation_ = DeviceOrientation::kLandscape;

  // Current observed desk.
  raw_ptr<const Desk, ExperimentalAsh> current_desk_ = nullptr;

  // Observed windows on the active desk.
  std::vector<aura::Window*> observed_windows_;

  // Windows that recovered by window restore have no parents at the initialize
  // stage, so their window states cannot be observed when are inserted into
  // `observed_windows_` list. This set contains the windows recovered by window
  // restored whose window states have not been observed yet.
  std::set<aura::Window*> no_state_observed_windows_;

  // Start time of clamshell and tablet split view. When stop recording, the
  // start time will be set to `base::TimeTicks::Max()`. This is also used as an
  // indicator of whether we are recording clamshell/tablet split view.
  base::TimeTicks clamshell_split_view_start_time_;
  base::TimeTicks tablet_split_view_start_time_;

  // An accumulator of clamshell split view engagement time. When the clamshell
  // split view with two windows snapped on both sides is paused (the definition
  // of "pause" is defined in the comments at the beginning), accumulate the
  // current engagement time period.
  int64_t clamshell_split_view_time_ = 0;

  // Counter of resizing windows in split view.
  int tablet_resize_count_ = 0;
  int clamshell_resize_count_ = 0;

  // `TabletModeController` is destroyed before `SplitViewMetricsController`.
  // Sets a `ScopedObservation` to help remove observer.
  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_controller_observation_{this};

  // Counter of swapping windows in split view.
  int swap_count_ = 0;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_
