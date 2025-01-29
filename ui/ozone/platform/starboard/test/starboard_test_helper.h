// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef UI_OZONE_PLATFORM_STARBOARD_TEST_STARBOARD_TEST_HELPER_H_
#define UI_OZONE_PLATFORM_STARBOARD_TEST_STARBOARD_TEST_HELPER_H_

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/thread.h"
#include "starboard/event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/platform_window/platform_window_delegate.h"

using ::starboard::ConditionVariable;
using ::starboard::Mutex;
using ::starboard::Thread;

namespace ui {
class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // Test ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {
    type = ui::EventTypeFromNative(event);
  }
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

  ui::EventType GetEventType() { return type; }

 private:
  ui::EventType type = ui::ET_UNKNOWN;
};
}  // namespace ui
#endif  // UI_OZONE_PLATFORM_STARBOARD_TEST_STARBOARD_TEST_HELPER_H_
