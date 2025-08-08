// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accessibility_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
AccessibilityController* g_instance = nullptr;
}

AccessibilityController* AccessibilityController::Get() {
  return g_instance;
}

AccessibilityController::AccessibilityController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AccessibilityController::~AccessibilityController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
