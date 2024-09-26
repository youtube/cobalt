// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/virtual_keyboard_controller_stub.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// VirtualKeyboardControllerStub member definitions.
VirtualKeyboardControllerStub::VirtualKeyboardControllerStub() = default;

VirtualKeyboardControllerStub::~VirtualKeyboardControllerStub() = default;

bool VirtualKeyboardControllerStub::DisplayVirtualKeyboard() {
  visible_ = true;
  for (auto& observer : observers_) {
    observer.OnKeyboardVisible({});
  }
  return true;
}

void VirtualKeyboardControllerStub::DismissVirtualKeyboard() {
  visible_ = false;
  for (auto& observer : observers_) {
    observer.OnKeyboardHidden();
  }
}

void VirtualKeyboardControllerStub::AddObserver(
    VirtualKeyboardControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void VirtualKeyboardControllerStub::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool VirtualKeyboardControllerStub::IsKeyboardVisible() {
  return visible_;
}

}  // namespace ui
