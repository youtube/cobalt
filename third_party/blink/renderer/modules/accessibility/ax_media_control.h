// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROL_H_

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

namespace blink {

class AXObjectCacheImpl;

class AccessibilityMediaControl : public AXLayoutObject {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);

  AccessibilityMediaControl(LayoutObject*, AXObjectCacheImpl&);
  AccessibilityMediaControl(const AccessibilityMediaControl&) = delete;
  AccessibilityMediaControl& operator=(const AccessibilityMediaControl&) =
      delete;
  ~AccessibilityMediaControl() override = default;

  // AXLayoutObject:
  bool InternalSetAccessibilityFocusAction() override;
  bool InternalClearAccessibilityFocusAction() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROL_H_
