// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

#include <cmath>
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/types/event_type.h"

namespace ui {
namespace {

class TestPlatformEventObserver : public ui::PlatformEventObserver {
 public:
  TestPlatformEventObserver() = default;
  ~TestPlatformEventObserver() override = default;

  void WillProcessEvent(const PlatformEvent& event) override {
    last_event_type_ = event->type();
    last_flags_ = event->flags();
    if (event->IsKeyEvent()) {
      const KeyEvent* key_event = event->AsKeyEvent();
      last_key_code_ = key_event->key_code();
    }
    if (event->IsMouseEvent()) {
      const MouseEvent* mouse_event = event->AsMouseEvent();
      last_location_ = mouse_event->location_f();
      last_changed_button_flags_ = mouse_event->changed_button_flags();
    }
    if (event->IsMouseWheelEvent()) {
      const MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
      last_wheel_offset_ = wheel_event->offset();
    }
  }

  void DidProcessEvent(const PlatformEvent& event) override {}

  absl::optional<ui::EventType> last_event_type() const {
    return last_event_type_;
  }
  absl::optional<int> last_flags() const { return last_flags_; }
  absl::optional<ui::KeyboardCode> last_key_code() const {
    return last_key_code_;
  }
  absl::optional<gfx::Vector2d> last_wheel_offset() const {
    return last_wheel_offset_;
  }
  absl::optional<gfx::PointF> last_location() const { return last_location_; }
  absl::optional<int> last_changed_button_flags() const {
    return last_changed_button_flags_;
  }

  void Reset() {
    last_event_type_.reset();
    last_flags_.reset();
    last_key_code_.reset();
    last_wheel_offset_.reset();
    last_location_.reset();
    last_changed_button_flags_.reset();
  }

 private:
  absl::optional<ui::EventType> last_event_type_;
  absl::optional<int> last_flags_;
  absl::optional<ui::KeyboardCode> last_key_code_;
  absl::optional<gfx::Vector2d> last_wheel_offset_;
  absl::optional<gfx::PointF> last_location_;
  absl::optional<int> last_changed_button_flags_;
};

class PlatformEventSourceStarboardTest : public ::testing::Test {
 protected:
  void SetUp() override { event_source_.AddPlatformEventObserver(&observer_); }

  void TearDown() override {
    event_source_.RemovePlatformEventObserver(&observer_);
  }

  void SendInputEvent(const SbInputData& input_data, int64_t timestamp = 1000) {
    observer_.Reset();
    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = const_cast<SbInputData*>(&input_data);
    sb_event.timestamp = timestamp;

    event_source_.HandleEvent(&sb_event);
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  PlatformEventSourceStarboard event_source_;
  TestPlatformEventObserver observer_;
};

TEST_F(PlatformEventSourceStarboardTest, HandleMouseWheelEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeWheel;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = 100.0f;
  input_data.position.y = 200.0f;
  input_data.delta.x = 2.0f;
  input_data.delta.y = -3.0f;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::ET_MOUSEWHEEL);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(100.0f, 200.0f));
  // In Chromium ui::MouseWheelEvent, offset.x > 0 is left, offset.y > 0 is up.
  // Starboard sends delta.x > 0 for right, delta.y > 0 for down.
  // Hence offset = -delta * kWheelDelta.
  EXPECT_EQ(observer_.last_wheel_offset(),
            gfx::Vector2d(-2 * ui::MouseWheelEvent::kWheelDelta,
                          3 * ui::MouseWheelEvent::kWheelDelta));
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
}

TEST_F(PlatformEventSourceStarboardTest, HandleKeyboardEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypePress;
  input_data.device_type = kSbInputDeviceTypeKeyboard;
  input_data.key = kSbKeySpace;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::ET_KEY_PRESSED);
  EXPECT_EQ(observer_.last_key_code(), ui::VKEY_SPACE);
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
}

}  // namespace
}  // namespace ui
