// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_
#define UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace wm {

// ShadowControllerDelegate allows a user to modify a shadow on certain windows
// differently from the normal use case.
class COMPONENT_EXPORT(UI_WM) ShadowControllerDelegate {
 public:
  ShadowControllerDelegate() = default;
  virtual ~ShadowControllerDelegate() = default;

  // Invoked when the shadow on |window| is to be modified, either normally from
  // activation change or manually.
  virtual bool ShouldShowShadowForWindow(const aura::Window* window) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_
