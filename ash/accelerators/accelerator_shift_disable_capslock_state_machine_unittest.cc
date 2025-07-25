// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_shift_disable_capslock_state_machine.h"

#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"

namespace ash::accelerators {

namespace {

class MockInputController : public ui::InputController {
 public:
  MOCK_METHOD(bool, AreAnyKeysPressed, (), (override));

 private:
  bool HasMouse() override { return false; }
  bool HasPointingStick() override { return false; }
  bool HasTouchpad() override { return false; }
  bool HasHapticTouchpad() override { return false; }
  bool IsCapsLockEnabled() override { return false; }
  void SetCapsLockEnabled(bool enabled) override {}
  void SetNumLockEnabled(bool enabled) override {}
  bool IsAutoRepeatEnabled() override { return true; }
  void SetAutoRepeatEnabled(bool enabled) override {}
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override {}
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override {}
  void SetCurrentLayoutByName(const std::string& layout_name) override {}
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetGamepadKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void DescribeForLog(DescribeForLogReply reply) const override {
    std::move(reply).Run(std::string());
  }
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void PlayHapticTouchpadEffect(
      ui::HapticTouchpadEffect effect_type,
      ui::HapticTouchpadEffectStrength strength) override {}
  void SetHapticTouchpadEffectForNextButtonRelease(
      ui::HapticTouchpadEffect effect_type,
      ui::HapticTouchpadEffectStrength strength) override {}
  void SetTouchpadSensitivity(absl::optional<int> device_id,
                              int value) override {}
  void SetTouchpadScrollSensitivity(absl::optional<int> device_id,
                                    int value) override {}
  void SetTouchpadHapticFeedback(absl::optional<int> device_id,
                                 bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(absl::optional<int> device_id,
                                         int value) override {}
  void SetTapToClick(absl::optional<int> device_id, bool enabled) override {}
  void SetTapDragging(absl::optional<int> device_id, bool enabled) override {}
  void SetNaturalScroll(absl::optional<int> device_id, bool enabled) override {}
  void SetMouseSensitivity(absl::optional<int> device_id, int value) override {}
  void SetMouseScrollSensitivity(absl::optional<int> device_id,
                                 int value) override {}
  void SetMouseReverseScroll(absl::optional<int> device_id,
                             bool enabled) override {}
  void SetMouseAcceleration(absl::optional<int> device_id,
                            bool enabled) override {}
  void SetMouseScrollAcceleration(absl::optional<int> device_id,
                                  bool enabled) override {}
  void SetPointingStickSensitivity(absl::optional<int> device_id,
                                   int value) override {}
  void SetPointingStickAcceleration(absl::optional<int> device_id,
                                    bool enabled) override {}
  void SetTouchpadAcceleration(absl::optional<int> device_id,
                               bool enabled) override {}
  void SetTouchpadScrollAcceleration(absl::optional<int> device_id,
                                     bool enabled) override {}
  void SetPrimaryButtonRight(absl::optional<int> device_id,
                             bool right) override {}
  void SetPointingStickPrimaryButtonRight(absl::optional<int> device_id,
                                          bool right) override {}
  void DisableKeyboardImposterCheck() override {}
};

const bool kKeysPressed = true;
const bool kNoKeysPressed = false;

using EventTypeVariant = absl::variant<ui::MouseEvent, ui::KeyEvent, bool>;
using ShiftDisableState =
    AcceleratorShiftDisableCapslockStateMachine::ShiftDisableState;

ui::MouseEvent MousePress() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                        /*location=*/gfx::PointF{},
                        /*root_location=*/gfx::PointF{},
                        /*time_stamp=*/{}, ui::EF_LEFT_MOUSE_BUTTON,
                        ui::EF_LEFT_MOUSE_BUTTON);
}

ui::MouseEvent MouseRelease() {
  return ui::MouseEvent(ui::ET_MOUSE_RELEASED,
                        /*location=*/gfx::PointF{},
                        /*root_location=*/gfx::PointF{},
                        /*time_stamp=*/{}, ui::EF_NONE,
                        ui::EF_LEFT_MOUSE_BUTTON);
}

ui::KeyEvent KeyPress(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
}

ui::KeyEvent KeyRelease(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::ET_KEY_RELEASED, key_code, ui::EF_NONE);
}

ui::Event& GetEventFromVariant(EventTypeVariant& event) {
  if (absl::holds_alternative<ui::MouseEvent>(event)) {
    return absl::get<ui::MouseEvent>(event);
  } else {
    return absl::get<ui::KeyEvent>(event);
  }
}

}  // namespace

class AcceleratorShiftDisableCapslockStateMachineTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<std::vector<EventTypeVariant>, ShiftDisableState>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    std::tie(events_, expected_state_) = GetParam();

    input_controller_ = std::make_unique<MockInputController>();
    shift_disable_state_machine_ =
        std::make_unique<AcceleratorShiftDisableCapslockStateMachine>(
            input_controller_.get());
  }

  void TearDown() override {
    shift_disable_state_machine_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<AcceleratorShiftDisableCapslockStateMachine>
      shift_disable_state_machine_;
  std::unique_ptr<MockInputController> input_controller_;
  std::vector<EventTypeVariant> events_;
  ShiftDisableState expected_state_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AcceleratorShiftDisableCapslockStateMachineTest,
    testing::ValuesIn(std::vector<std::tuple<std::vector<EventTypeVariant>,
                                             ShiftDisableState>>{
        // Shift -> Release Shift allows the disable to trigger.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), kNoKeysPressed,
          KeyRelease(ui::VKEY_SHIFT)},
         ShiftDisableState::kTrigger},

        // A -> A release goes back to starting state.
        {{kKeysPressed, KeyPress(ui::VKEY_A), kNoKeysPressed,
          KeyRelease(ui::VKEY_A)},
         ShiftDisableState::kStart},

        // Pressing A moves to suppress state.
        {{kKeysPressed, KeyPress(ui::VKEY_A)}, ShiftDisableState::kSuppress},

        // A -> Shift -> Release shift keeps us in suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_A), KeyPress(ui::VKEY_SHIFT),
          KeyRelease(ui::VKEY_SHIFT)},
         ShiftDisableState::kSuppress},

        // If any key is released, stay in starting state.
        {{KeyRelease(ui::VKEY_A)}, ShiftDisableState::kStart},

        // If shift is presesd twice, stay in primed state.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), KeyPress(ui::VKEY_RSHIFT)},
         ShiftDisableState::kPrimed},

        // If any key is pressed once in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LSHIFT), KeyPress(ui::VKEY_A)},
         ShiftDisableState::kSuppress},

        // If any key is released once in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LSHIFT), KeyRelease(ui::VKEY_0)},
         ShiftDisableState::kSuppress},

        // If mouse is pressed in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), MousePress()},
         ShiftDisableState::kSuppress},

        // If mouse is pressed and released, ensure we end up back in starting
        // state.
        {{MousePress(), MouseRelease()}, ShiftDisableState::kStart},
    }));

TEST_P(AcceleratorShiftDisableCapslockStateMachineTest, StateTest) {
  for (auto& event : events_) {
    if (absl::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(absl::get<bool>(event)));
      continue;
    }

    shift_disable_state_machine_->OnEvent(&GetEventFromVariant(event));
  }

  EXPECT_EQ(expected_state_, shift_disable_state_machine_->current_state());
}

}  // namespace ash::accelerators
