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

class MockPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  // Mock ui::PlatformWindowDelegate implementation.
  MockPlatformWindowDelegate();

  MockPlatformWindowDelegate(const MockPlatformWindowDelegate&) = delete;
  MockPlatformWindowDelegate& operator=(const MockPlatformWindowDelegate&) =
      delete;

  ~MockPlatformWindowDelegate() override;

  MOCK_METHOD(void, OnBoundsChanged, (const BoundsChange& change), (override));
  MOCK_METHOD(void,
              OnDamageRect,
              (const gfx::Rect& damaged_region),
              (override));
  MOCK_METHOD(void, DispatchEvent, (ui::Event * event), (override));
  MOCK_METHOD(void, OnCloseRequest, (), (override));
  MOCK_METHOD(void, OnClosed, (), (override));
  MOCK_METHOD(void,
              OnWindowStateChanged,
              (ui::PlatformWindowState old_state,
               ui::PlatformWindowState new_state),
              (override));
  MOCK_METHOD(void, OnLostCapture, (), (override));
  MOCK_METHOD(void,
              OnAcceleratedWidgetAvailable,
              (gfx::AcceleratedWidget widget),
              (override));
  MOCK_METHOD(void, OnWillDestroyAcceleratedWidget, (), (override));
  MOCK_METHOD(void, OnAcceleratedWidgetDestroyed, (), (override));
  MOCK_METHOD(void, OnActivationChanged, (bool active), (override));
  MOCK_METHOD(void, OnMouseEnter, (), (override));
};

class OzoneStarboardTest : public testing::Test {
 public:
  OzoneStarboardTest();

  virtual ~OzoneStarboardTest();

  virtual void EventHandleInternal(const SbEvent* event);

 private:
  class OzoneStarboardThread : public Thread {
   public:
    OzoneStarboardThread() : Thread("sb_thread") {}

    void Run() override;
  };

  // These are used to initialize the main Starboard thread, which must be
  // running before some Starboard functions can be called.
  std::unique_ptr<OzoneStarboardThread> sb_main_;
  std::unique_ptr<ConditionVariable> started_condition_;
  Mutex started_mutex_;
};
}  // namespace ui
#endif  // UI_OZONE_PLATFORM_STARBOARD_TEST_STARBOARD_TEST_HELPER_H_
