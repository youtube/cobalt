// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_controller.h"

#include "base/memory/raw_ptr.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_touchpad_haptics.h"

namespace ui {

namespace {

class WaylandInputController : public InputController {
 public:
  explicit WaylandInputController(WaylandConnection* connection)
      : connection_(connection) {
    DCHECK(connection_);
  }

  WaylandInputController(const WaylandInputController&) = delete;
  WaylandInputController& operator=(const WaylandInputController&) = delete;

  ~WaylandInputController() override = default;

  // InputController:
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
  void SetTouchpadSensitivity(absl::optional<int> device_id,
                              int value) override {}
  void SetTouchpadScrollSensitivity(absl::optional<int> device_id,
                                    int value) override {}
  void SetTapToClick(absl::optional<int> device_id, bool enabled) override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetTapDragging(absl::optional<int> device_id, bool enabled) override {}
  void SetNaturalScroll(absl::optional<int> device_id, bool enabled) override {}
  void SetMouseSensitivity(absl::optional<int> device_id, int value) override {}
  void SetMouseScrollSensitivity(absl::optional<int> device_id,
                                 int value) override {}
  void SetPrimaryButtonRight(absl::optional<int> device_id,
                             bool right) override {}
  void SetMouseReverseScroll(absl::optional<int> device_id,
                             bool enabled) override {}
  void SetMouseAcceleration(absl::optional<int> device_id,
                            bool enabled) override {}
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
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
  void SetTouchpadAcceleration(absl::optional<int> device_id,
                               bool enabled) override {}
  void SetTouchpadScrollAcceleration(absl::optional<int> device_id,
                                     bool enabled) override {}
  void SetTouchpadHapticFeedback(absl::optional<int> device_id,
                                 bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(absl::optional<int> device_id,
                                         int value) override {}
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void SetInternalKeyboardFilter(bool enable_filter,
                                 std::vector<DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void PlayHapticTouchpadEffect(
      HapticTouchpadEffect effect_type,
      HapticTouchpadEffectStrength strength) override {
    auto* touchpad_haptics = connection_->zcr_touchpad_haptics();
    touchpad_haptics->Play(static_cast<int>(effect_type),
                           static_cast<int>(strength));
  }
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect_type,
      HapticTouchpadEffectStrength strength) override {
    // TODO(b:205702807) Implement after adding to wayland protocol
    NOTIMPLEMENTED_LOG_ONCE();
  }

 private:
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace

std::unique_ptr<InputController> CreateWaylandInputController(
    WaylandConnection* connection) {
  return std::make_unique<WaylandInputController>(connection);
}

}  // namespace ui
