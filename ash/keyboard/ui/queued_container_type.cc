// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/queued_container_type.h"

#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"

namespace keyboard {

QueuedContainerType::QueuedContainerType(
    KeyboardUIController* controller,
    ContainerType container_type,
    gfx::Rect bounds,
    base::OnceCallback<void(bool success)> callback)
    : controller_(controller),
      container_type_(container_type),
      bounds_(bounds),
      callback_(std::move(callback)) {}

QueuedContainerType::~QueuedContainerType() {
  bool change_successful =
      controller_->GetActiveContainerType() == container_type_;
  std::move(callback_).Run(change_successful);
}

}  // namespace keyboard
