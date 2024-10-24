// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_

#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

class PlatformWindowStarboard : public StubWindow, public PlatformEventDispatcher {
 public:
  PlatformWindowStarboard(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);

  PlatformWindowStarboard(const PlatformWindowStarboard&) = delete;
  PlatformWindowStarboard& operator=(const PlatformWindowStarboard&) = delete;

  ~PlatformWindowStarboard() override;

  // PlatformEventDispatcher implementation:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
