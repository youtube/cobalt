// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_PRIORITY_HINT_H_
#define UI_GFX_OVERLAY_PRIORITY_HINT_H_

namespace gfx {

// Provides a hint to a system compositor how it should prioritize this
// overlay. Used only by Wayland.
enum OverlayPriorityHint {
  // Overlay promotion is not necessary for this surface.
  kNone = 0,
  // The overlay could be considered as a candidate for promotion.
  kRegular,
  // Low latency quad.
  kLowLatencyCanvas,
  // The overlay contains protected content and requires to be promoted to
  // overlay.
  kHardwareProtection,
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_PRIORITY_HINT_H_
