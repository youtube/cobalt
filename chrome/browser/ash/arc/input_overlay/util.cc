// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/util.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "base/notreached.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

int ModifierDomCodeToEventFlag(ui::DomCode code) {
  switch (code) {
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return ui::EF_ALT_DOWN;
    case ui::DomCode::CAPS_LOCK:
      return ui::EF_CAPS_LOCK_ON;
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      return ui::EF_COMMAND_DOWN;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return ui::EF_SHIFT_DOWN;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return ui::EF_CONTROL_DOWN;
    default:
      return ui::EF_NONE;
  }
}

bool IsSameDomCode(ui::DomCode a, ui::DomCode b) {
  return a == b ||
         (ModifierDomCodeToEventFlag(a) != ui::EF_NONE &&
          ModifierDomCodeToEventFlag(a) == ModifierDomCodeToEventFlag(b));
}

MouseAction ConvertToMouseActionEnum(const std::string& mouse_action) {
  if (mouse_action == kPrimaryClick) {
    return MouseAction::PRIMARY_CLICK;
  } else if (mouse_action == kSecondaryClick) {
    return MouseAction::SECONDARY_CLICK;
  } else if (mouse_action == kHoverMove) {
    return MouseAction::HOVER_MOVE;
  } else if (mouse_action == kPrimaryDragMove) {
    return MouseAction::PRIMARY_DRAG_MOVE;
  } else if (mouse_action == kSecondaryDragMove) {
    return MouseAction::SECONDARY_DRAG_MOVE;
  }
  return MouseAction::NONE;
}

bool UpdatePositionByArrowKey(ui::KeyboardCode key, gfx::Point& position) {
  switch (key) {
    case ui::VKEY_LEFT:
      position.set_x(position.x() - kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_RIGHT:
      position.set_x(position.x() + kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_UP:
      position.set_y(position.y() - kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_DOWN:
      position.set_y(position.y() + kArrowKeyMoveDistance);
      return true;
    default:
      return false;
  }
}

InputElement* GetInputBindingByBindingOption(Action* action,
                                             BindingOption binding_option) {
  InputElement* input_binding = nullptr;
  switch (binding_option) {
    case BindingOption::kCurrent:
      input_binding = action->current_input();
      break;
    case BindingOption::kOriginal:
      input_binding = action->original_input();
      break;
    case BindingOption::kPending:
      input_binding = action->pending_input();
      break;
    default:
      NOTREACHED();
  }
  return input_binding;
}

void ClampPosition(gfx::Point& position,
                   const gfx::Size& ui_size,
                   const gfx::Size& parent_size,
                   int parent_padding) {
  int lo = parent_padding;
  int hi = parent_size.width() - ui_size.width() - parent_padding;
  if (lo >= hi) {
    // Ignore |parent_padding| if there is not enough space.
    lo = 0;
    hi += parent_padding;
  }
  position.set_x(std::clamp(position.x(), lo, hi));

  lo = parent_padding;
  hi = parent_size.height() - ui_size.height() - parent_padding;
  if (lo >= hi) {
    // Ignore |parent_padding| if there is not enough space.
    lo = 0;
    hi += parent_padding;
  }
  position.set_y(std::clamp(position.y(), lo, hi));
}

absl::optional<std::string> GetCurrentSystemVersion() {
  return AllowReposition() ? absl::make_optional(kSystemVersionAlphaV2)
                           : absl::nullopt;
}

void ResetFocusTo(views::View* view) {
  DCHECK(view);
  auto* focus_manager = view->GetFocusManager();
  if (!focus_manager) {
    return;
  }
  focus_manager->SetFocusedView(view);
}

bool AllowReposition() {
  return ash::features::IsArcInputOverlayAlphaV2Enabled() ||
         ash::features::IsArcInputOverlayBetaEnabled();
}

}  // namespace arc::input_overlay
