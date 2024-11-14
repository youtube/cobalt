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

#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

// TODO(b/371272304): Stop extending StubWindow and create a more robust window
// implementation.
// TODO(b/371272304): Add event handling (i.e. extend PlatformEventDispatcher).
class PlatformWindowStarboard : public StubWindow {
 public:
  PlatformWindowStarboard(PlatformWindowDelegate* delegate,
                          const gfx::Rect& bounds);

  PlatformWindowStarboard(const PlatformWindowStarboard&) = delete;
  PlatformWindowStarboard& operator=(const PlatformWindowStarboard&) = delete;

  ~PlatformWindowStarboard() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_PLATFORM_WINDOW_STARBOARD_H_
