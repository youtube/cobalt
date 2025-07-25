// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsDispatcher
    : InputDeviceSettingsController::Observer {
 public:
  explicit InputDeviceSettingsDispatcher(ui::InputController* input_controller);
  InputDeviceSettingsDispatcher(const InputDeviceSettingsDispatcher&) = delete;
  InputDeviceSettingsDispatcher& operator=(
      const InputDeviceSettingsDispatcher&) = delete;
  ~InputDeviceSettingsDispatcher() override;

  // InputDeviceSettingsController::Observer:
  void OnMouseConnected(const mojom::Mouse& mouse) override;
  void OnMouseSettingsUpdated(const mojom::Mouse& mouse) override;
  void OnTouchpadConnected(const mojom::Touchpad& touchpad) override;
  void OnTouchpadSettingsUpdated(const mojom::Touchpad& touchpad) override;
  void OnPointingStickConnected(
      const mojom::PointingStick& pointing_stick) override;
  void OnPointingStickSettingsUpdated(
      const mojom::PointingStick& pointing_stick) override;

 private:
  void DispatchMouseSettings(const mojom::Mouse& mouse);
  void DispatchPointingStickSettings(
      const mojom::PointingStick& pointing_stick);
  void DispatchTouchpadSettings(const mojom::Touchpad& touchpad);

  raw_ptr<ui::InputController> input_controller_;  // Not Owned
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_DISPATCHER_H_
