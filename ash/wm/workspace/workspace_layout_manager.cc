// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <algorithm>
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

using ::chromeos::WindowStateType;

WorkspaceLayoutManager::FloatingWindowObserver::FloatingWindowObserver(
    WorkspaceLayoutManager* workspace_layout_manager)
    : workspace_layout_manager_(workspace_layout_manager) {}

WorkspaceLayoutManager::FloatingWindowObserver::~FloatingWindowObserver() {
  for (auto window : observed_windows_)
    window.first->RemoveObserver(this);
}

void WorkspaceLayoutManager::FloatingWindowObserver::ObserveWindow(
    aura::Window* window) {
  if (!observed_windows_.count(window)) {
    observed_windows_[window] = window->parent();
    window->AddObserver(this);
  }
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  auto* old_parent = observed_windows_[params.target];
  if (params.new_parent && params.new_parent != old_parent) {
    StopOberservingWindow(params.target);
  }
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  workspace_layout_manager_->NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  StopOberservingWindow(window);
}

void WorkspaceLayoutManager::FloatingWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  workspace_layout_manager_->NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::FloatingWindowObserver::StopOberservingWindow(
    aura::Window* window) {
  observed_windows_.erase(window);
  window->RemoveObserver(this);
}

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : window_(window),
      root_window_(window->GetRootWindow()),
      root_window_controller_(RootWindowController::ForWindow(root_window_)),
      floating_window_observer_(this),
      work_area_in_parent_(
          screen_util::GetDisplayWorkAreaBoundsInParent(window_)),
      is_fullscreen_(GetWindowForFullscreenModeForContext(window) != nullptr) {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  root_window_->AddObserver(this);
  backdrop_controller_ = std::make_unique<BackdropController>(window_);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  settings_bubble_container_ = window->GetRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);
  accessibility_bubble_container_ = window->GetRootWindow()->GetChildById(
      kShellWindowId_AccessibilityBubbleContainer);
  shelf_container_ =
      window->GetRootWindow()->GetChildById(kShellWindowId_ShelfContainer);
  root_window_controller_->shelf()->AddObserver(this);
  Shell::Get()->app_list_controller()->AddObserver(this);
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  // WorkspaceLayoutManagers for the primary display are destroyed after
  // AppListControllerImpl. Their observers are removed in OnShellDestroying().
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  root_window_controller_->shelf()->RemoveObserver(this);
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (aura::Window* window : windows_) {
    WindowState* window_state = WindowState::Get(window);
    window_state->RemoveObserver(this);
    window->RemoveObserver(this);
  }
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK_NE(aura::client::WINDOW_TYPE_CONTROL, child->GetType());
  WindowState* window_state = WindowState::Get(child);
  WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
  windows_.insert(child);
  child->AddObserver(this);
  window_state->AddObserver(this);
  UpdateShelfVisibility();
  UpdateFullscreenState();
  UpdateWindowWorkspace(child);

  backdrop_controller_->OnWindowAddedToLayout(child);
  window_positioner::RearrangeVisibleWindowOnShow(child);
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    WindowState::Get(child)->DisableZOrdering(nullptr);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  WindowState* window_state = WindowState::Get(child);
  window_state->RemoveObserver(this);

  // When a window is removing from a workspace layout, it is going to be added
  // to a new workspace layout or destroyed.
  if (!window_state->pre_added_to_workspace_window_bounds()) {
    if (window_state->pre_auto_manage_window_bounds()) {
      window_state->SetPreAddedToWorkspaceWindowBounds(
          *window_state->pre_auto_manage_window_bounds());
    } else {
      window_state->SetPreAddedToWorkspaceWindowBounds(child->bounds());
    }
  }

  if (child->layer()->GetTargetVisibility())
    window_positioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  backdrop_controller_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                            bool visible) {
  WindowState* window_state = WindowState::Get(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->layer()->GetTargetVisibility())
    window_positioner::RearrangeVisibleWindowOnShow(child);
  else
    window_positioner::RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateFullscreenState();
  UpdateShelfVisibility();
  backdrop_controller_->OnChildWindowVisibilityChanged(child);
}

void WorkspaceLayoutManager::SetChildBounds(aura::Window* child,
                                            const gfx::Rect& requested_bounds) {
  WindowState* window_state = WindowState::Get(child);
  SetBoundsWMEvent event(requested_bounds);
  window_state->OnWMEvent(&event);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ash::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& new_bounds) {
  auto* keyboard_window =
      keyboard::KeyboardUIController::Get()->GetKeyboardWindow();
  if (keyboard_window && keyboard_window->GetRootWindow() == root_window_)
    NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::OnKeyboardDisplacingBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  aura::Window* window = window_util::GetActiveWindow();
  if (!window)
    return;

  window = window->GetToplevelWindow();
  if (!window_->Contains(window))
    return;

  WindowState* window_state = WindowState::Get(window);
  if (window_state->ignore_keyboard_bounds_change())
    return;

  if (!new_bounds_in_screen.IsEmpty()) {
    // Store existing bounds to be restored before resizing for keyboard if it
    // is not already stored.
    if (!window_state->HasRestoreBounds())
      window_state->SaveCurrentBoundsForRestore();

    gfx::Rect window_bounds(window->GetTargetBounds());
    ::wm::ConvertRectToScreen(window_, &window_bounds);
    int vertical_displacement =
        std::max(0, window_bounds.bottom() - new_bounds_in_screen.y());
    int shift = std::min(vertical_displacement,
                         window_bounds.y() - work_area_in_parent_.y());
    if (shift > 0) {
      gfx::Point origin(window_bounds.x(), window_bounds.y() - shift);
      SetChildBounds(window, gfx::Rect(origin, window_bounds.size()));
    }
  } else if (window_state->HasRestoreBounds()) {
    // Keyboard hidden, restore original bounds if they exist. If the user has
    // resized or dragged the window in the meantime, WorkspaceWindowResizer
    // will have cleared the restore bounds and this code will not accidentally
    // override user intent.
    window_state->SetAndClearRestoreBounds();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent) {
    if (params.new_parent == settings_bubble_container_.get() ||
        params.new_parent == accessibility_bubble_container_.get() ||
        IsPopupNotificationWindow(params.target)) {
      floating_window_observer_.ObserveWindow(params.target);
    }
  }
  // The window should have a parent (unless it's being removed), so we can
  // create WindowState, which requires its parent. (crbug.com/924305)
  // TODO(oshima): Change this to |EnsureWindowState|, then change
  // GetWindowState so that it simply returns the WindowState associated with
  // the window, or nullptr.
  if (params.new_parent)
    WindowState::Get(params.target);

  if (!wm::IsActiveWindow(params.target))
    return;
  // If the window is already tracked by the workspace this update would be
  // redundant as the fullscreen and shelf state would have been handled in
  // OnWindowAddedToLayout.
  if (windows_.find(params.target) != windows_.end())
    return;

  // If the active window has moved to this root window then update the
  // fullscreen state.
  // TODO(flackr): Track the active window leaving this root window and update
  // the fullscreen state accordingly.
  if (params.new_parent && params.new_parent->GetRootWindow() == root_window_) {
    UpdateFullscreenState();
    UpdateShelfVisibility();
  }
}

void WorkspaceLayoutManager::OnWindowAdded(aura::Window* window) {
  if (window->parent() == settings_bubble_container_ ||
      window->parent() == accessibility_bubble_container_ ||
      IsPopupNotificationWindow(window)) {
    floating_window_observer_.ObserveWindow(window);
  }
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kZOrderingKey) {
    if (window->GetProperty(aura::client::kZOrderingKey) !=
        ui::ZOrderLevel::kNormal) {
      aura::Window* container =
          root_window_controller_->always_on_top_controller()->GetContainer(
              window);
      if (window->parent() != container)
        container->AddChild(window);
    }
  } else if (key == kWindowBackdropKey) {
    // kWindowBackdropKey is not supposed to be cleared.
    DCHECK(window->GetProperty(kWindowBackdropKey));
  } else if (key == aura::client::kWindowWorkspaceKey) {
    UpdateWindowWorkspace(window);
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  backdrop_controller_->OnWindowStackingChanged(window);
}

void WorkspaceLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }
  if (settings_bubble_container_ == window)
    settings_bubble_container_ = nullptr;
  if (accessibility_bubble_container_ == window)
    accessibility_bubble_container_ = nullptr;
  if (shelf_container_ == window)
    shelf_container_ = nullptr;
  Shell::Get()->desks_controller()->MaybeRemoveVisibleOnAllDesksWindow(window);
}

void WorkspaceLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::ActivationChangeObserver implementation:

void WorkspaceLayoutManager::OnWindowActivating(ActivationReason reason,
                                                aura::Window* gaining_active,
                                                aura::Window* losing_active) {
  if (windows_.find(gaining_active) == windows_.end()) {
    return;
  }

  WindowState* window_state =
      gaining_active ? WindowState::Get(gaining_active) : nullptr;
  if (window_state && window_state->IsMinimized() &&
      !gaining_active->IsVisible()) {
    window_state->Unminimize();
  }
}

void WorkspaceLayoutManager::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  // This callback may be called multiple times with one activation change
  // because we have one instance of this class for each desk.
  // TODO(b/265746505): Make sure to avoid redundant calls.
  if (lost_active)
    WindowState::Get(lost_active)->OnActivationLost();

  UpdateFullscreenState();
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, WindowStateObserver implementation:

void WorkspaceLayoutManager::OnPostWindowStateTypeChange(
    WindowState* window_state,
    WindowStateType old_type) {
  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() ||
      old_type == WindowStateType::kFullscreen) {
    UpdateFullscreenState();
  }

  UpdateShelfVisibility();
  backdrop_controller_->OnPostWindowStateTypeChange(window_state->window());
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, display::DisplayObserver implementation:

void WorkspaceLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id() !=
      display.id()) {
    return;
  }

  if (changed_metrics & (display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         display::DisplayObserver::DISPLAY_METRIC_PRIMARY)) {
    const DisplayMetricsChangedWMEvent wm_event(changed_metrics);
    AdjustAllWindowsBoundsForWorkAreaChange(&wm_event);
  }

  const gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInParent(window_));
  if (work_area != work_area_in_parent_) {
    const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&event);
  }
  backdrop_controller_->OnDisplayMetricsChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShellObserver implementation:

void WorkspaceLayoutManager::OnFullscreenStateChanged(bool is_fullscreen,
                                                      aura::Window* container) {
  // Note that only the active desk's container broadcasts this event, but all
  // containers' workspaces (active desk's and inactive desks' as well the
  // always-on-top container) receive it.
  DCHECK(desks_util::IsActiveDeskContainer(container));

  // If |container| is the one associated with this workspace, then fullscreen
  // state must match.
  DCHECK(window_ != container || is_fullscreen == is_fullscreen_);

  // This notification may come from active desk containers on other displays.
  // No need to update the always-on-top states if the fullscreen state change
  // happened on a different root window.
  if (container->GetRootWindow() != root_window_)
    return;

  if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    // If this is in pinned mode, then this event does not trigger the
    // always-on-top state change, because it is kept disabled regardless of
    // the fullscreen state change.
    return;
  }

  // We need to update the always-on-top states even for inactive desks
  // containers, because inactive desks may have a previously demoted
  // always-on-top windows that we need to promote back to the always-on-top
  // container if there no longer fullscreen windows on this root window.
  UpdateAlwaysOnTop(GetWindowForFullscreenModeInRoot(root_window_));
}

void WorkspaceLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  const bool is_pinned = Shell::Get()->screen_pinning_controller()->IsPinned();
  if (!is_pinned && is_fullscreen_) {
    // On exiting from pinned mode, if the workspace is still in fullscreen
    // mode, then this event does not trigger the restoring yet. On exiting
    // from fullscreen, the temporarily disabled always-on-top property will be
    // restored.
    return;
  }

  UpdateAlwaysOnTop(is_pinned ? pinned_window : nullptr);
}

void WorkspaceLayoutManager::OnShellDestroying() {
  Shell::Get()->app_list_controller()->RemoveObserver(this);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShelfObserver implementation:

void WorkspaceLayoutManager::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  NotifySystemUiAreaChanged();
}

void WorkspaceLayoutManager::OnHotseatStateChanged(HotseatState old_state,
                                                   HotseatState new_state) {
  NotifySystemUiAreaChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, AppListControllerObserver implementation:

void WorkspaceLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                        int64_t display_id) {
  if (display::Screen::GetScreen()->GetDisplayNearestWindow(window_).id() !=
      display_id) {
    return;
  }
  if (!Shell::Get()->IsInTabletMode()) {
    // Adjust PIP window if needed.
    NotifySystemUiAreaChanged();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    const WMEvent* event) {
  DCHECK(event->type() == WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  work_area_in_parent_ = screen_util::GetDisplayWorkAreaBoundsInParent(window_);

  // The PIP avoids the accessibility bubbles, so here we update the
  // accessibility position before sending the WMEvent, so that if the PIP is
  // also being shown the PIPs calculation does not need to take place twice.
  NotifyAccessibilityWorkspaceChanged();

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (aura::Window* window : windows_)
    WindowState::Get(window)->OnWMEvent(event);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  root_window_controller_->shelf()->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // Note that we don't allow always-on-top or PiP containers to have fullscreen
  // windows, and we only update the fullscreen state for the active desk
  // container.
  if (!desks_util::IsActiveDeskContainer(window_))
    return;

  const bool is_fullscreen =
      GetWindowForFullscreenModeForContext(window_) != nullptr;
  if (is_fullscreen == is_fullscreen_)
    return;

  is_fullscreen_ = is_fullscreen;
  Shell::Get()->NotifyFullscreenStateChanged(is_fullscreen, window_);
}

void WorkspaceLayoutManager::UpdateAlwaysOnTop(
    aura::Window* active_desk_fullscreen_window) {
  // Changing always on top state may change window's parent. Iterate on a copy
  // of |windows_| to avoid invalidating an iterator. Since both workspace and
  // always_on_top containers' layouts are managed by this class all the
  // appropriate windows will be included in the iteration.
  // Use an `aura::WindowTracker` since `OnWillRemoveWindowFromLayout()` may
  // remove windows from `windows_`.
  std::vector<aura::Window*> windows(windows_.begin(), windows_.end());
  aura::WindowTracker tracker(windows);
  while (!tracker.windows().empty()) {
    aura::Window* window = tracker.Pop();
    if (window == active_desk_fullscreen_window)
      continue;

    WindowState* window_state = WindowState::Get(window);
    if (active_desk_fullscreen_window)
      window_state->DisableZOrdering(active_desk_fullscreen_window);
    else
      window_state->RestoreZOrdering();
  }
}

void WorkspaceLayoutManager::NotifySystemUiAreaChanged() const {
  // The PIP avoids the accessibility bubble, so here we update the
  // accessibility bubble position before sending the WMEvent, so that if the
  // PIP is also being shown the PIPs calculation does not need to take place
  // twice.
  NotifyAccessibilityWorkspaceChanged();
  for (auto* window : windows_) {
    WMEvent event(WM_EVENT_SYSTEM_UI_AREA_CHANGED);
    WindowState::Get(window)->OnWMEvent(&event);
  }
}

void WorkspaceLayoutManager::NotifyAccessibilityWorkspaceChanged() const {
  Shell::Get()->accessibility_controller()->UpdateFloatingPanelBoundsIfNeeded();
}

void WorkspaceLayoutManager::UpdateWindowWorkspace(aura::Window* window) {
  if (window->GetType() != aura::client::WindowType::WINDOW_TYPE_NORMAL ||
      window->GetProperty(aura::client::kZOrderingKey) !=
          ui::ZOrderLevel::kNormal) {
    return;
  }

  auto* desks_controller = Shell::Get()->desks_controller();
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window))
    desks_controller->AddVisibleOnAllDesksWindow(window);
  else
    desks_controller->MaybeRemoveVisibleOnAllDesksWindow(window);
}

bool WorkspaceLayoutManager::IsPopupNotificationWindow(
    aura::Window* window) const {
  return window->parent() == shelf_container_ &&
         window->GetName() ==
             AshMessagePopupCollection::kMessagePopupWidgetName;
}

}  // namespace ash
