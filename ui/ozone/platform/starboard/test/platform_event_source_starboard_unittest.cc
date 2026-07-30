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
#include <optional>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_observer.h"

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

  std::optional<ui::EventType> last_event_type() const {
    return last_event_type_;
  }
  std::optional<int> last_flags() const { return last_flags_; }
  std::optional<ui::KeyboardCode> last_key_code() const {
    return last_key_code_;
  }
  std::optional<gfx::Vector2d> last_wheel_offset() const {
    return last_wheel_offset_;
  }
  std::optional<gfx::PointF> last_location() const { return last_location_; }
  std::optional<int> last_changed_button_flags() const {
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
  std::optional<ui::EventType> last_event_type_;
  std::optional<int> last_flags_;
  std::optional<ui::KeyboardCode> last_key_code_;
  std::optional<gfx::Vector2d> last_wheel_offset_;
  std::optional<gfx::PointF> last_location_;
  std::optional<int> last_changed_button_flags_;
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

// --- Tests for CreateKeyboardRemoteInputEvent ---

TEST(PlatformEventSourceStarboardHelpersTest, CreateKeyboardRemoteInputEvent) {
  PlatformEventSourceStarboard event_source;

  // Key press on keyboard.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeKeyboard;
    input_data.key = kSbKeyA;
    input_data.key_modifiers = kSbKeyModifiersShift;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 5000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateKeyboardRemoteInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kKeyPressed);
    EXPECT_EQ(event->flags(), ui::EF_SHIFT_DOWN);

    const ui::KeyEvent* key_event = event->AsKeyEvent();
    ASSERT_NE(key_event, nullptr);
    EXPECT_EQ(key_event->key_code(), ui::VKEY_A);
  }

  // Key release on remote.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeRemote;
    input_data.key = kSbKeyEscape;
    input_data.key_modifiers =
        kSbKeyModifiersCtrl | kSbKeyModifiersAlt | kSbKeyModifiersMeta;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 6000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateKeyboardRemoteInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kKeyReleased);
    EXPECT_EQ(event->flags(),
              ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

    const ui::KeyEvent* key_event = event->AsKeyEvent();
    ASSERT_NE(key_event, nullptr);
    EXPECT_EQ(key_event->key_code(), ui::VKEY_ESCAPE);
  }

  // Invalid event type for keyboard should return nullptr.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeKeyboard;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;

    std::unique_ptr<ui::Event> event =
        event_source.CreateKeyboardRemoteInputEvent(&sb_event);
    EXPECT_EQ(event, nullptr);
  }
}

// --- Tests for CreateMouseInputEvent ---

TEST(PlatformEventSourceStarboardHelpersTest, CreateMouseInputEvent) {
  PlatformEventSourceStarboard event_source;

  // Mouse move without buttons held.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 100.0f;
    input_data.position.y = 200.0f;
    input_data.key_modifiers = kSbKeyModifiersCtrl;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 1000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kMouseMoved);
    EXPECT_EQ(event->flags(), ui::EF_CONTROL_DOWN);

    const ui::MouseEvent* mouse_event = event->AsMouseEvent();
    ASSERT_NE(mouse_event, nullptr);
    EXPECT_EQ(mouse_event->location_f(), gfx::PointF(100.0f, 200.0f));
    EXPECT_EQ(mouse_event->changed_button_flags(), 0);
  }

  // Mouse move with Left button held (drag) - becomes kMouseDragged in
  // Chromium.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 110.0f;
    input_data.position.y = 210.0f;
    input_data.key_modifiers = kSbKeyModifiersPointerButtonLeft;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kMouseDragged);
    EXPECT_EQ(event->flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Mouse press with kSbKeyMouse1 (Left button).
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 150.0f;
    input_data.position.y = 250.0f;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 2000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kMousePressed);
    EXPECT_EQ(event->flags(), ui::EF_LEFT_MOUSE_BUTTON);

    const ui::MouseEvent* mouse_event = event->AsMouseEvent();
    ASSERT_NE(mouse_event, nullptr);
    EXPECT_EQ(mouse_event->location_f(), gfx::PointF(150.0f, 250.0f));
    EXPECT_EQ(mouse_event->changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Mouse release with kSbKeyMouse1.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 150.0f;
    input_data.position.y = 250.0f;
    input_data.key_modifiers = kSbKeyModifiersShift;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 3000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(event->flags(), ui::EF_SHIFT_DOWN);

    const ui::MouseEvent* mouse_event = event->AsMouseEvent();
    ASSERT_NE(mouse_event, nullptr);
    EXPECT_EQ(mouse_event->changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Wheel events are handled in HandleEvent via CreateMouseWheelInputEvent,
  // so CreateMouseInputEvent should return nullptr.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeWheel;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 80.0f;
    input_data.position.y = 90.0f;
    input_data.delta.x = 1.0f;
    input_data.delta.y = -2.0f;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    EXPECT_EQ(event, nullptr);
  }

  // Unsupported input event type returns nullptr.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeInput;
    input_data.device_type = kSbInputDeviceTypeMouse;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseInputEvent(&sb_event);
    EXPECT_EQ(event, nullptr);
  }
}

// --- Tests for CreateMouseWheelInputEvent ---

TEST(PlatformEventSourceStarboardHelpersTest, CreateMouseWheelInputEvent) {
  PlatformEventSourceStarboard event_source;

  // Test integer and floating point scaling with std::round.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeWheel;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 200.0f;
    input_data.position.y = 300.0f;
    input_data.delta.x = 3.0f;
    input_data.delta.y = -1.5f;
    input_data.key_modifiers = kSbKeyModifiersAlt;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 4000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseWheelInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kMousewheel);
    EXPECT_EQ(event->flags(), ui::EF_ALT_DOWN);

    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
    ASSERT_NE(wheel_event, nullptr);
    EXPECT_EQ(wheel_event->location_f(), gfx::PointF(200.0f, 300.0f));
    EXPECT_EQ(wheel_event->offset(), gfx::Vector2d(-360, 180));
  }

  // Test fractional smooth scroll rounding (-0.005 * 120 = -0.6 -> -1, with
  // sign negation becomes 1).
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeWheel;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.delta.x = -0.005f;  // -(-0.6) -> 1
    input_data.delta.y = 0.005f;   // -(0.6) -> -1

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;

    std::unique_ptr<ui::Event> event =
        event_source.CreateMouseWheelInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
    ASSERT_NE(wheel_event, nullptr);
    EXPECT_EQ(wheel_event->offset(), gfx::Vector2d(1, -1));
  }
}

// --- Tests for CreateTouchInputEvent ---

TEST(PlatformEventSourceStarboardHelpersTest, CreateTouchInputEvent) {
  PlatformEventSourceStarboard event_source;

  // Touch press with valid pressure.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeTouchScreen;
    input_data.position.x = 400.0f;
    input_data.position.y = 500.0f;
    input_data.device_id = 1;
    input_data.size.x = 12.0f;
    input_data.size.y = 14.0f;
    input_data.pressure = 0.8f;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 7000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateTouchInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kTouchPressed);

    const ui::TouchEvent* touch_event = event->AsTouchEvent();
    ASSERT_NE(touch_event, nullptr);
    EXPECT_EQ(touch_event->location_f(), gfx::PointF(400.0f, 500.0f));
    EXPECT_FLOAT_EQ(touch_event->pointer_details().force, 0.8f);
    EXPECT_FLOAT_EQ(touch_event->pointer_details().radius_x, 12.0f);
    EXPECT_FLOAT_EQ(touch_event->pointer_details().radius_y, 14.0f);
    EXPECT_EQ(touch_event->pointer_details().id, 1);
  }

  // Touch move with NaN pressure clamped to minimum 0.5f.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeTouchPad;
    input_data.position.x = 410.0f;
    input_data.position.y = 510.0f;
    input_data.pressure = 0.2f;  // clamped to 0.5f

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 8000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateTouchInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kTouchMoved);

    const ui::TouchEvent* touch_event = event->AsTouchEvent();
    ASSERT_NE(touch_event, nullptr);
    EXPECT_FLOAT_EQ(touch_event->pointer_details().force, 0.5f);
  }

  // Touch unpress.
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeTouchScreen;
    input_data.position.x = 410.0f;
    input_data.position.y = 510.0f;

    SbEvent sb_event = {};
    sb_event.type = kSbEventTypeInput;
    sb_event.data = &input_data;
    sb_event.timestamp = 9000;

    std::unique_ptr<ui::Event> event =
        event_source.CreateTouchInputEvent(&sb_event);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->type(), ui::EventType::kTouchReleased);
  }
}

// --- End-to-end integration tests with HandleEvent ---

// Verifies that a normal mouse move without buttons held does NOT have
// EF_LEFT_MOUSE_BUTTON.
TEST_F(PlatformEventSourceStarboardTest, HandleMouseMoveEventWithoutButtons) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeMove;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = 150.0f;
  input_data.position.y = 250.0f;
  input_data.key_modifiers = 0;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseMoved);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(150.0f, 250.0f));
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  EXPECT_EQ(observer_.last_changed_button_flags(), 0);
}

// Verifies that mouse drag (move with Left Button held) correctly sets
// EF_LEFT_MOUSE_BUTTON and becomes kMouseDragged in Chromium.
TEST_F(PlatformEventSourceStarboardTest, HandleMouseDragEventWithLeftButton) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeMove;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = 150.0f;
  input_data.position.y = 250.0f;
  input_data.key_modifiers = kSbKeyModifiersPointerButtonLeft;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseDragged);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(150.0f, 250.0f));
  EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(PlatformEventSourceStarboardTest, HandleMousePressEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypePress;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.key = kSbKeyMouse1;
  input_data.position.x = 100.0f;
  input_data.position.y = 200.0f;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(100.0f, 200.0f));
  EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(PlatformEventSourceStarboardTest, HandleMouseReleaseEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeUnpress;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.key = kSbKeyMouse1;
  input_data.position.x = 120.0f;
  input_data.position.y = 220.0f;
  input_data.key_modifiers = 0;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(120.0f, 220.0f));
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(PlatformEventSourceStarboardTest, HandleMouseWheelEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeWheel;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = 100.0f;
  input_data.position.y = 200.0f;
  input_data.delta.x = 2.0f;
  input_data.delta.y = -3.0f;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousewheel);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(100.0f, 200.0f));
  // In Chromium ui::MouseWheelEvent, offset.x > 0 is left, offset.y > 0 is up.
  // Starboard sends delta.x > 0 for right, delta.y > 0 for down.
  // Hence offset = -delta * kWheelDelta.
  EXPECT_EQ(observer_.last_wheel_offset(),
            gfx::Vector2d(-2 * ui::MouseWheelEvent::kWheelDelta,
                          3 * ui::MouseWheelEvent::kWheelDelta));
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
}

TEST_F(PlatformEventSourceStarboardTest, HandleMouseWheelEvent_NonMouseDevice) {
  const SbInputDeviceType kNonMouseDeviceTypes[] = {
      kSbInputDeviceTypeTouchPad,
      kSbInputDeviceTypeRemote,
  };

  for (SbInputDeviceType device_type : kNonMouseDeviceTypes) {
    SCOPED_TRACE(device_type);
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeWheel;
    input_data.device_type = device_type;
    input_data.position.x = 150.0f;
    input_data.position.y = 250.0f;
    input_data.delta.x = 0.0f;
    input_data.delta.y = 1.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousewheel);
    EXPECT_EQ(observer_.last_location(), gfx::PointF(150.0f, 250.0f));
    EXPECT_EQ(observer_.last_wheel_offset(),
              gfx::Vector2d(0, -1 * ui::MouseWheelEvent::kWheelDelta));
  }
}

TEST_F(PlatformEventSourceStarboardTest, HandleKeyboardEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypePress;
  input_data.device_type = kSbInputDeviceTypeKeyboard;
  input_data.key = kSbKeySpace;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kKeyPressed);
  EXPECT_EQ(observer_.last_key_code(), ui::VKEY_SPACE);
  EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
}

TEST_F(PlatformEventSourceStarboardTest, HandleTouchEvent) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypePress;
  input_data.device_type = kSbInputDeviceTypeTouchScreen;
  input_data.position.x = 300.0f;
  input_data.position.y = 400.0f;

  SendInputEvent(input_data);

  EXPECT_EQ(observer_.last_event_type(), ui::EventType::kTouchPressed);
}

// Verifies that browser navigation keys from mouse/pointer devices are routed
// as standard mouse back/forward button events.
TEST_F(PlatformEventSourceStarboardTest,
       HandleBrowserNavigationKeysFromPointerDevice) {
  // 1. Browser Back Press -> MousePressed with EF_BACK_MOUSE_BUTTON
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyBrowserBack;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_BACK_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_BACK_MOUSE_BUTTON);
  }

  // 2. Browser Back Release -> MouseReleased with EF_NONE
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyBrowserBack;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_BACK_MOUSE_BUTTON);
  }

  // 3. Browser Forward Press -> MousePressed with EF_FORWARD_MOUSE_BUTTON
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyBrowserForward;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_FORWARD_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(),
              ui::EF_FORWARD_MOUSE_BUTTON);
  }

  // 4. Browser Forward Release -> MouseReleased with EF_NONE
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyBrowserForward;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
    EXPECT_EQ(observer_.last_changed_button_flags(),
              ui::EF_FORWARD_MOUSE_BUTTON);
  }
}

// --- Edge Case Tests for Button Tracking & State Management ---

// Verifies chorded mouse clicks: Left Press -> Right Press -> Left Release ->
// Right Release.
TEST_F(PlatformEventSourceStarboardTest, HandleChordedMouseButtons) {
  // 1. Press Left Button
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 100.0f;
    input_data.position.y = 100.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // 2. Press Right Button while Left Button is held
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse3;
    input_data.position.x = 100.0f;
    input_data.position.y = 100.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(),
              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
  }

  // 3. Release Left Button while Right Button is still held
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 100.0f;
    input_data.position.y = 100.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // 4. Release Right Button
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse3;
    input_data.position.x = 100.0f;
    input_data.position.y = 100.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
  }
}

// Verifies fallback when platform omits key on Unpress (e.g. key = kSbKeyNone).
// Ensures button state is cleanly released and does not remain stuck.
TEST_F(PlatformEventSourceStarboardTest, HandleUnpressWithOmittedKeyFallback) {
  // 1. Press Right Button
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse3;
    input_data.position.x = 50.0f;
    input_data.position.y = 60.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
  }

  // 2. Subsequent Move (without modifiers from platform) -> tracked drag
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 55.0f;
    input_data.position.y = 65.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseDragged);
    EXPECT_EQ(observer_.last_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
  }

  // 3. Unpress with key = kSbKeyUnknown and key_modifiers = 0
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyUnknown;
    input_data.position.x = 55.0f;
    input_data.position.y = 65.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
    EXPECT_EQ(observer_.last_changed_button_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
  }

  // 4. Subsequent Move is clean kMouseMoved without stuck buttons
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 60.0f;
    input_data.position.y = 70.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseMoved);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  }
}

// Verifies that mouse drag works when platforms only send button press and move
// without repeating key_modifiers on every move event.
TEST_F(PlatformEventSourceStarboardTest,
       HandleMouseMoveDragWithoutPlatformModifiers) {
  // Press Left button without modifiers
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 10.0f;
    input_data.position.y = 20.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Move without modifiers -> should be kMouseDragged
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 15.0f;
    input_data.position.y = 25.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseDragged);
    EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Release Left button
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 15.0f;
    input_data.position.y = 25.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  }

  // Subsequent move -> should be kMouseMoved
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 20.0f;
    input_data.position.y = 30.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseMoved);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  }
}

// Verifies that window blur (loss of focus) clears all tracked mouse buttons,
// preventing buttons from remaining stuck if the release event was missed.
TEST_F(PlatformEventSourceStarboardTest,
       HandleFocusLossBlurClearsPressedMouseButtons) {
  // Press Left button
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = kSbKeyMouse1;
    input_data.position.x = 100.0f;
    input_data.position.y = 100.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_flags(), ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Window loses focus (Blur event)
  {
    SbEvent blur_event = {};
    blur_event.type = kSbEventTypeBlur;
    event_source_.HandleFocusEvent(&blur_event);
    base::RunLoop().RunUntilIdle();
  }

  // Subsequent move should not be a drag and have no button flags
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeMove;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.position.x = 110.0f;
    input_data.position.y = 110.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseMoved);
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
  }
}

// --- Parameterized tests for Keyboard modifiers with SendInputEvent ---

struct KeyboardModifierTestCase {
  const char* name;
  unsigned int sb_modifiers;
  int expected_ui_flags;
};

class PlatformEventSourceStarboardKeyboardModifierTest
    : public PlatformEventSourceStarboardTest,
      public ::testing::WithParamInterface<KeyboardModifierTestCase> {};

TEST_P(PlatformEventSourceStarboardKeyboardModifierTest,
       HandleKeyboardModifiers) {
  const auto& param = GetParam();

  // Test Key Press with modifier
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeKeyboard;
    input_data.key = kSbKeyA;
    input_data.key_modifiers = param.sb_modifiers;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kKeyPressed);
    EXPECT_EQ(observer_.last_key_code(), ui::VKEY_A);
    EXPECT_EQ(observer_.last_flags(), param.expected_ui_flags);
  }

  // Test Key Release with modifier
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeKeyboard;
    input_data.key = kSbKeyA;
    input_data.key_modifiers = param.sb_modifiers;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kKeyReleased);
    EXPECT_EQ(observer_.last_key_code(), ui::VKEY_A);
    EXPECT_EQ(observer_.last_flags(), param.expected_ui_flags);
  }
}

INSTANTIATE_TEST_SUITE_P(
    KeyboardModifiers,
    PlatformEventSourceStarboardKeyboardModifierTest,
    ::testing::Values(
        KeyboardModifierTestCase{"NoModifiers", 0, ui::EF_NONE},
        KeyboardModifierTestCase{"Shift", kSbKeyModifiersShift,
                                 ui::EF_SHIFT_DOWN},
        KeyboardModifierTestCase{"Control", kSbKeyModifiersCtrl,
                                 ui::EF_CONTROL_DOWN},
        KeyboardModifierTestCase{"Alt", kSbKeyModifiersAlt, ui::EF_ALT_DOWN},
        KeyboardModifierTestCase{"Meta", kSbKeyModifiersMeta,
                                 ui::EF_COMMAND_DOWN},
        KeyboardModifierTestCase{"ShiftAndCtrl",
                                 kSbKeyModifiersShift | kSbKeyModifiersCtrl,
                                 ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
        KeyboardModifierTestCase{"AltAndMeta",
                                 kSbKeyModifiersAlt | kSbKeyModifiersMeta,
                                 ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN},
        KeyboardModifierTestCase{"AllModifiers",
                                 kSbKeyModifiersShift | kSbKeyModifiersCtrl |
                                     kSbKeyModifiersAlt | kSbKeyModifiersMeta,
                                 ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                     ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN}),
    [](const ::testing::TestParamInfo<KeyboardModifierTestCase>& info) {
      return info.param.name;
    });

// --- Parameterized tests for Pointer Button modifiers during Mouse Move/Drag
// with SendInputEvent ---

struct PointerButtonModifierTestCase {
  const char* name;
  unsigned int sb_modifiers;
  int expected_ui_flags;
};

class PlatformEventSourceStarboardPointerButtonTest
    : public PlatformEventSourceStarboardTest,
      public ::testing::WithParamInterface<PointerButtonModifierTestCase> {};

TEST_P(PlatformEventSourceStarboardPointerButtonTest, HandlePointerButtonDrag) {
  const auto& param = GetParam();

  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeMove;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = 200.0f;
  input_data.position.y = 300.0f;
  input_data.key_modifiers = param.sb_modifiers;

  SendInputEvent(input_data);

  ui::EventType expected_type = param.expected_ui_flags == ui::EF_NONE
                                    ? ui::EventType::kMouseMoved
                                    : ui::EventType::kMouseDragged;
  EXPECT_EQ(observer_.last_event_type(), expected_type);
  EXPECT_EQ(observer_.last_location(), gfx::PointF(200.0f, 300.0f));
  EXPECT_EQ(observer_.last_flags(), param.expected_ui_flags);
}

INSTANTIATE_TEST_SUITE_P(
    PointerButtons,
    PlatformEventSourceStarboardPointerButtonTest,
    ::testing::Values(
        PointerButtonModifierTestCase{"None", 0, ui::EF_NONE},
        PointerButtonModifierTestCase{"LeftButton",
                                      kSbKeyModifiersPointerButtonLeft,
                                      ui::EF_LEFT_MOUSE_BUTTON},
        PointerButtonModifierTestCase{"RightButton",
                                      kSbKeyModifiersPointerButtonRight,
                                      ui::EF_RIGHT_MOUSE_BUTTON},
        PointerButtonModifierTestCase{"MiddleButton",
                                      kSbKeyModifiersPointerButtonMiddle,
                                      ui::EF_MIDDLE_MOUSE_BUTTON},
        PointerButtonModifierTestCase{"BackButton",
                                      kSbKeyModifiersPointerButtonBack,
                                      ui::EF_BACK_MOUSE_BUTTON},
        PointerButtonModifierTestCase{"ForwardButton",
                                      kSbKeyModifiersPointerButtonForward,
                                      ui::EF_FORWARD_MOUSE_BUTTON},
        PointerButtonModifierTestCase{
            "LeftAndRight",
            kSbKeyModifiersPointerButtonLeft |
                kSbKeyModifiersPointerButtonRight,
            ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON}),
    [](const ::testing::TestParamInfo<PointerButtonModifierTestCase>& info) {
      return info.param.name;
    });

// --- Parameterized tests for Mouse Button KeyCodes (kSbKeyMouse1 -
// kSbKeyMouse5) with SendInputEvent ---

struct MouseButtonKeyTestCase {
  const char* name;
  SbKey key;
  int expected_button_flag;
};

class PlatformEventSourceStarboardMouseButtonKeyTest
    : public PlatformEventSourceStarboardTest,
      public ::testing::WithParamInterface<MouseButtonKeyTestCase> {};

TEST_P(PlatformEventSourceStarboardMouseButtonKeyTest,
       HandleMouseButtonPressAndRelease) {
  const auto& param = GetParam();

  // Test Press
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypePress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = param.key;
    input_data.position.x = 100.0f;
    input_data.position.y = 200.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMousePressed);
    EXPECT_EQ(observer_.last_location(), gfx::PointF(100.0f, 200.0f));
    EXPECT_EQ(observer_.last_flags(), param.expected_button_flag);
    EXPECT_EQ(observer_.last_changed_button_flags(),
              param.expected_button_flag);
  }

  // Test Release
  {
    SbInputData input_data = {};
    input_data.type = kSbInputEventTypeUnpress;
    input_data.device_type = kSbInputDeviceTypeMouse;
    input_data.key = param.key;
    input_data.position.x = 100.0f;
    input_data.position.y = 200.0f;

    SendInputEvent(input_data);

    EXPECT_EQ(observer_.last_event_type(), ui::EventType::kMouseReleased);
    EXPECT_EQ(observer_.last_location(), gfx::PointF(100.0f, 200.0f));
    EXPECT_EQ(observer_.last_flags(), ui::EF_NONE);
    EXPECT_EQ(observer_.last_changed_button_flags(),
              param.expected_button_flag);
  }
}

INSTANTIATE_TEST_SUITE_P(
    MouseButtonKeys,
    PlatformEventSourceStarboardMouseButtonKeyTest,
    ::testing::Values(MouseButtonKeyTestCase{"Mouse1_Left", kSbKeyMouse1,
                                             ui::EF_LEFT_MOUSE_BUTTON},
                      MouseButtonKeyTestCase{"Mouse2_Middle", kSbKeyMouse2,
                                             ui::EF_MIDDLE_MOUSE_BUTTON},
                      MouseButtonKeyTestCase{"Mouse3_Right", kSbKeyMouse3,
                                             ui::EF_RIGHT_MOUSE_BUTTON},
                      MouseButtonKeyTestCase{"Mouse4_Back", kSbKeyMouse4,
                                             ui::EF_BACK_MOUSE_BUTTON},
                      MouseButtonKeyTestCase{"Mouse5_Forward", kSbKeyMouse5,
                                             ui::EF_FORWARD_MOUSE_BUTTON}),
    [](const ::testing::TestParamInfo<MouseButtonKeyTestCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace ui
