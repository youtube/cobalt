// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_

#include "ui/views/views_export.h"

namespace ui {
class Event;
}

namespace views {

class InputEventActivationProtector;

// Delegate interface for evaluating if an input event should be blocked
// as a possibly unintended interaction. This allows incorporating additional
// signals into the protection check.
class VIEWS_EXPORT InputProtectorDelegate {
 public:
  virtual ~InputProtectorDelegate() = default;

  // Returns true if the `event` should be blocked based on the delegate's
  // logic. `protector` provides access to the calling protector's state.
  virtual bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      InputEventActivationProtector* protector) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_
