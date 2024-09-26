// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/host/ash_window_tree_host_platform.h"

#include "ash/host/ash_window_tree_host_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ash {

namespace {

class TestInputController : public ui::InputController {
 public:
  TestInputController() = default;
  TestInputController(const TestInputController&) = delete;
  TestInputController& operator=(const TestInputController&) = delete;
  ~TestInputController() override = default;

  // InputController:
  void SuspendMouseAcceleration() override { acceleration_suspended_ = true; }
  void EndMouseAccelerationSuspension() override {
    acceleration_suspended_ = false;
  }

  // these are all stubs that are not used yet in these tests
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
  void SetThreeFingerClick(bool enabled) override {}
  void SetTouchpadSensitivity(absl::optional<int> device_id,
                              int value) override {}
  void SetTouchpadScrollSensitivity(absl::optional<int> device_id,
                                    int value) override {}
  void SetTapToClick(absl::optional<int> device_id, bool enabled) override {}
  void SetTapDragging(absl::optional<int> device_id, bool enabled) override {}
  void SetNaturalScroll(absl::optional<int> device_id, bool enabled) override {}
  void SetTouchpadAcceleration(absl::optional<int> device_id,
                               bool enabled) override {}
  void SetTouchpadScrollAcceleration(absl::optional<int> device_id,
                                     bool enabled) override {}
  void SetTouchpadHapticFeedback(absl::optional<int> device_id,
                                 bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(absl::optional<int> device_id,
                                         int value) override {}
  void SetMouseSensitivity(absl::optional<int> device_id, int value) override {}
  void SetMouseScrollSensitivity(absl::optional<int> device_id,
                                 int value) override {}
  void SetPrimaryButtonRight(absl::optional<int> device_id,
                             bool right) override {}
  void SetMouseReverseScroll(absl::optional<int> device_id,
                             bool enabled) override {}
  void SetMouseAcceleration(absl::optional<int> device_id,
                            bool enabled) override {}
  void SetMouseScrollAcceleration(absl::optional<int> device_id,
                                  bool enabled) override {}
  void SetPointingStickSensitivity(absl::optional<int> device_id,
                                   int value) override {}
  void SetPointingStickPrimaryButtonRight(absl::optional<int> device_id,
                                          bool right) override {}
  void SetPointingStickAcceleration(absl::optional<int> device_id,
                                    bool enabled) override {}
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetGamepadKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTapToClickPaused(bool state) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    // Return that there is no stylus in the garage; this test class
    // does not need to trigger stylus charging behaviours.
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
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
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}

  bool GetAccelerationSuspended() { return acceleration_suspended_; }

 private:
  // member variable used to keep track of mouse acceleration suspension
  bool acceleration_suspended_ = false;
};

class FakeAshWindowTreeHostDelegate : public AshWindowTreeHostDelegate {
 public:
  FakeAshWindowTreeHostDelegate() = default;
  FakeAshWindowTreeHostDelegate(const FakeAshWindowTreeHostDelegate&) = delete;
  FakeAshWindowTreeHostDelegate& operator=(
      const FakeAshWindowTreeHostDelegate&) = delete;
  ~FakeAshWindowTreeHostDelegate() override = default;

  const display::Display* GetDisplayById(int64_t display_id) const override {
    return nullptr;
  }
  void SetCurrentEventTargeterSourceHost(aura::WindowTreeHost*) override {}
};

}  // namespace

class AshWindowTreeHostPlatformTest : public AshTestBase {
 public:
  AshWindowTreeHostPlatformTest() = default;
  AshWindowTreeHostPlatformTest(const AshWindowTreeHostPlatformTest&) = delete;
  AshWindowTreeHostPlatformTest& operator=(
      const AshWindowTreeHostPlatformTest&) = delete;
  ~AshWindowTreeHostPlatformTest() override = default;
};

TEST_F(AshWindowTreeHostPlatformTest, UnadjustedMovement) {
  FakeAshWindowTreeHostDelegate fake_delegate;
  auto stub = std::make_unique<ui::StubWindow>(gfx::Rect());
  auto* stub_ptr = stub.get();
  AshWindowTreeHostPlatform host(std::move(stub), &fake_delegate);
  stub_ptr->InitDelegate(&host, false);

  auto test_input_controller = std::make_unique<TestInputController>();
  host.input_controller_ = test_input_controller.get();

  std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
      unadjusted_movement_context = host.RequestUnadjustedMovement();
  EXPECT_TRUE(unadjusted_movement_context.get() != nullptr);
  EXPECT_TRUE(test_input_controller->GetAccelerationSuspended());
  unadjusted_movement_context.reset();
  EXPECT_FALSE(test_input_controller->GetAccelerationSuspended());
}

}  // namespace ash
