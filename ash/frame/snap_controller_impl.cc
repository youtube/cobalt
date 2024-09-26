// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/snap_controller_impl.h"

#include "ash/utility/haptics_util.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ui/aura/window.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

SnapControllerImpl::SnapControllerImpl() = default;
SnapControllerImpl::~SnapControllerImpl() = default;

bool SnapControllerImpl::CanSnap(aura::Window* window) {
  return WindowState::Get(window)->CanSnap();
}

void SnapControllerImpl::ShowSnapPreview(aura::Window* window,
                                         chromeos::SnapDirection snap,
                                         bool allow_haptic_feedback) {
  if (snap == chromeos::SnapDirection::kNone) {
    phantom_window_controller_.reset();
    return;
  }

  if (!phantom_window_controller_ ||
      phantom_window_controller_->window() != window) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(window);
  }
  const SnapViewType snap_type = snap == chromeos::SnapDirection::kPrimary
                                     ? SnapViewType::kPrimary
                                     : SnapViewType::kSecondary;
  gfx::Rect phantom_bounds_in_screen =
      GetDefaultSnappedWindowBoundsInParent(window, snap_type);
  ::wm::ConvertRectToScreen(window->parent(), &phantom_bounds_in_screen);

  const bool need_haptic_feedback =
      allow_haptic_feedback &&
      phantom_window_controller_->GetTargetWindowBounds() !=
          phantom_bounds_in_screen;

  phantom_window_controller_->Show(phantom_bounds_in_screen);

  // Fire a haptic event if necessary.
  if (need_haptic_feedback) {
    haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kSnap,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void SnapControllerImpl::CommitSnap(aura::Window* window,
                                    chromeos::SnapDirection snap,
                                    float snap_ratio) {
  phantom_window_controller_.reset();
  if (snap == chromeos::SnapDirection::kNone)
    return;

  WindowState* window_state = WindowState::Get(window);
  window_state->set_snap_action_source(
      WindowSnapActionSource::kUseCaptionButtonToSnap);
  const WMEvent snap_event(snap == chromeos::SnapDirection::kPrimary
                               ? WM_EVENT_SNAP_PRIMARY
                               : WM_EVENT_SNAP_SECONDARY,
                           snap_ratio);
  window_state->OnWMEvent(&snap_event);
}

}  // namespace ash
