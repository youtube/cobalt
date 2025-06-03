// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_LAYOUT_MANAGER_H_
#define ASH_WM_LOCK_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class WindowState;
class WMEvent;

// LockLayoutManager is used for the windows created in LockScreenContainer.
// For Chrome OS this includes out-of-box/login/lock/multi-profile login use
// cases. LockScreenContainer does not use default work area definition.
// By default work area is defined as display area minus shelf, and minus
// virtual keyboard bounds.
// For windows in LockScreenContainer work area is display area minus virtual
// keyboard bounds (only if keyboard overscroll is disabled). If keyboard
// overscroll is enabled then work area always equals to display area size since
// virtual keyboard changes inner workspace of each WebContents.
// For all windows in LockScreenContainer default WindowState is replaced
// with LockWindowState.
class ASH_EXPORT LockLayoutManager : public WmDefaultLayoutManager,
                                     public aura::WindowObserver,
                                     public display::DisplayObserver,
                                     public KeyboardControllerObserver {
 public:
  explicit LockLayoutManager(aura::Window* window);

  LockLayoutManager(const LockLayoutManager&) = delete;
  LockLayoutManager& operator=(const LockLayoutManager&) = delete;

  ~LockLayoutManager() override;

  // Overridden from WmDefaultLayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // Overriden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // KeyboardControllerObserver overrides:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override;

 protected:
  // Adjusts the bounds of all managed windows when the display area changes.
  // This happens when the display size, work area insets has changed.
  void AdjustWindowsForWorkAreaChange(const WMEvent* event);

  aura::Window* window() { return window_; }

 private:
  raw_ptr<aura::Window, ExperimentalAsh> window_;
  raw_ptr<aura::Window, ExperimentalAsh> root_window_;

  // An observer to update position of modals when display work area changes.
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_LOCK_LAYOUT_MANAGER_H_
