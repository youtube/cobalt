// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OverviewWallpaperController;

// Manages a overview session which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT OverviewController : public OverviewDelegate,
                                      public ::wm::ActivationChangeObserver {
 public:
  OverviewController();

  OverviewController(const OverviewController&) = delete;
  OverviewController& operator=(const OverviewController&) = delete;

  ~OverviewController() override;

  // Starts/Ends overview with `type`. Returns true if enter or exit overview
  // successful. Depending on `type` the enter/exit animation will look
  // different. `action` is used by UMA to record the reasons that trigger
  // overview starts or ends. E.g, pressing the overview button.
  bool StartOverview(
      OverviewStartAction action,
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);
  bool EndOverview(OverviewEndAction action,
                   OverviewEnterExitType type = OverviewEnterExitType::kNormal);

  // Returns true if overview mode is active.
  bool InOverviewSession() const;

  // Moves the current selection forward or backward.
  void IncrementSelection(bool forward);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Returns true if we're in start-overview animation.
  bool IsInStartAnimation();

  // Returns true if overview has been shutdown, but is still animating to the
  // end state ui.
  bool IsCompletingShutdownAnimations() const;

  // Pause or unpause the occlusion tracker. Resets the unpause delay if we were
  // already in the process of unpausing.
  void PauseOcclusionTracker();
  void UnpauseOcclusionTracker(base::TimeDelta delay);

  void AddObserver(OverviewObserver* observer);
  void RemoveObserver(OverviewObserver* observer);

  // Post a task to update the shadow and rounded corners of overview windows.
  void DelayedUpdateRoundedCornersAndShadow();

  // OverviewDelegate:
  void AddExitAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyExitAnimationObserver(
      DelayedAnimationObserver* animation) override;
  void AddEnterAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override;
  void RemoveAndDestroyEnterAnimationObserver(
      DelayedAnimationObserver* animation_observer) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

  OverviewSession* overview_session() { return overview_session_.get(); }

  bool disable_app_id_check_for_saved_desks() const {
    return disable_app_id_check_for_saved_desks_;
  }

  void set_occlusion_pause_duration_for_end_for_test(base::TimeDelta duration) {
    occlusion_pause_duration_for_end_ = duration;
  }
  void set_delayed_animation_task_delay_for_test(base::TimeDelta delta) {
    delayed_animation_task_delay_ = delta;
  }

  // Gets the windows list that are shown in the overview windows grids if the
  // overview mode is active for testing.
  std::vector<aura::Window*> GetWindowsListInOverviewGridsForTest();

 private:
  friend class SavedDeskTest;

  void set_disable_app_id_check_for_saved_desks(bool val) {
    disable_app_id_check_for_saved_desks_ = val;
  }

  // Toggle overview mode. Depending on |type| the enter/exit animation will
  // look different.
  void ToggleOverview(
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);

  // Returns true if it's possible to enter or exit overview mode in the current
  // configuration. This can be false at certain times, such as when the lock
  // screen is visible we can't overview mode.
  bool CanEnterOverview();
  bool CanEndOverview(OverviewEnterExitType type);

  void OnStartingAnimationComplete(bool canceled);
  void OnEndingAnimationComplete(bool canceled);
  void ResetPauser();

  void UpdateRoundedCornersAndShadow();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  // Collection of DelayedAnimationObserver objects. When this becomes empty,
  // notify shell that the starting animations have been completed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> start_animations_;

  // Indicates that overview shall gain focus when the starting animations have
  // completed.
  bool should_focus_overview_ = false;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;

  std::unique_ptr<OverviewSession> overview_session_;
  base::Time last_overview_session_time_;

  base::TimeDelta occlusion_pause_duration_for_end_;

  // Handles blurring and dimming of the wallpaper when entering or exiting
  // overview mode. Animates the blurring and dimming if necessary.
  std::unique_ptr<OverviewWallpaperController> overview_wallpaper_controller_;

  base::CancelableOnceClosure reset_pauser_task_;

  // App dragging enters overview right away. This task is used to delay the
  // |OnStartingAnimationComplete| call so that some animations do not make the
  // initial setup less performant.
  base::TimeDelta delayed_animation_task_delay_;

  base::ObserverList<OverviewObserver> observers_;

  std::unique_ptr<views::Widget::PaintAsActiveLock> paint_as_active_lock_;

  // In ash unittests, the `FullRestoreSaveHandler` isn't hooked up so
  // initialized windows lack an app id. If a window doesn't have a valid app
  // id, then it won't be tracked by `OverviewGrid` as a supported window and
  // those windows will be deemed unsupported for Saved Desks. If
  // `disable_app_id_check_for_saved_desks_` is true, then this check is
  // omitted so we can test Saved Desks.
  bool disable_app_id_check_for_saved_desks_ = false;

  base::WeakPtrFactory<OverviewController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
