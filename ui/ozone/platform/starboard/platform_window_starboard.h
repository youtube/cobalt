// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_

#include "starboard/window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/ozone/platform/starboard/platform_event_observer_starboard.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

class PlatformWindowStarboard
    : public StubWindow,
      public PlatformEventDispatcher,
      public starboard::PlatformEventObserverStarboard {
 public:
  PlatformWindowStarboard(PlatformWindowDelegate* delegate,
                          const gfx::Rect& bounds);

  PlatformWindowStarboard(const PlatformWindowStarboard&) = delete;
  PlatformWindowStarboard& operator=(const PlatformWindowStarboard&) = delete;

  ~PlatformWindowStarboard() override;

  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  bool ShouldUseNativeFrame() const override;
  void SetUseNativeFrame(bool use_native_frame) override;

  // ui::PlatformEventObserverStarboard interface.
  void ProcessWindowSizeChangedEvent(int width, int height) override;

 private:
  SbWindow sb_window_;
  bool use_native_frame_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
