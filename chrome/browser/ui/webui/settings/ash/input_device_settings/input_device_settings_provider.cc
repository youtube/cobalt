// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace ash::settings {

namespace {

template <typename T>
struct CustomDeviceKeyComparator {
  bool operator()(const T& device1, const T& device2) const {
    return device1->device_key < device2->device_key;
  }
};

template <typename T>
std::vector<T> SanitizeAndSortDeviceList(std::vector<T> devices) {
  // Remove devices with duplicate `device_key`.
  base::flat_set<T, CustomDeviceKeyComparator<T>> devices_no_duplicates_set(
      std::move(devices));
  std::vector<T> devices_no_duplicates =
      std::move(devices_no_duplicates_set).extract();
  base::ranges::sort(devices_no_duplicates,
                     [](const auto& device1, const auto& device2) {
                       // Guarantees that external devices appear first in the
                       // list.
                       if (device1->is_external != device2->is_external) {
                         return device1->is_external;
                       }

                       // Otherwise sort by most recently connected device (aka
                       // id in descending order).
                       return device1->id > device2->id;
                     });
  return devices_no_duplicates;
}

}  // namespace

InputDeviceSettingsProvider::InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (features::IsInputDeviceSettingsSplitEnabled() && controller) {
    controller->AddObserver(this);
  }
}

InputDeviceSettingsProvider::~InputDeviceSettingsProvider() {
  auto* controller = InputDeviceSettingsController::Get();
  if (features::IsInputDeviceSettingsSplitEnabled() && controller) {
    controller->RemoveObserver(this);
  }
}

void InputDeviceSettingsProvider::BindInterface(
    mojo::PendingReceiver<mojom::InputDeviceSettingsProvider> receiver) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void InputDeviceSettingsProvider::SetKeyboardSettings(
    uint32_t device_id,
    ::ash::mojom::KeyboardSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetKeyboardSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::SetPointingStickSettings(
    uint32_t device_id,
    ::ash::mojom::PointingStickSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetPointingStickSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::SetMouseSettings(
    uint32_t device_id,
    ::ash::mojom::MouseSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetMouseSettings(device_id,
                                                         std::move(settings));
}

void InputDeviceSettingsProvider::SetTouchpadSettings(
    uint32_t device_id,
    ::ash::mojom::TouchpadSettingsPtr settings) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  InputDeviceSettingsController::Get()->SetTouchpadSettings(
      device_id, std::move(settings));
}

void InputDeviceSettingsProvider::ObserveKeyboardSettings(
    mojo::PendingRemote<mojom::KeyboardSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = keyboard_settings_observers_.Add(std::move(observer));
  auto* keyboard_settings_observer = keyboard_settings_observers_.Get(id);
  keyboard_settings_observer->OnKeyboardListUpdated(SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards()));
  keyboard_settings_observer->OnKeyboardPoliciesUpdated(
      InputDeviceSettingsController::Get()->GetKeyboardPolicies().Clone());
}

void InputDeviceSettingsProvider::ObserveTouchpadSettings(
    mojo::PendingRemote<mojom::TouchpadSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = touchpad_settings_observers_.Add(std::move(observer));
  touchpad_settings_observers_.Get(id)->OnTouchpadListUpdated(
      SanitizeAndSortDeviceList(
          InputDeviceSettingsController::Get()->GetConnectedTouchpads()));
}

void InputDeviceSettingsProvider::ObservePointingStickSettings(
    mojo::PendingRemote<mojom::PointingStickSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = pointing_stick_settings_observers_.Add(std::move(observer));
  pointing_stick_settings_observers_.Get(id)->OnPointingStickListUpdated(
      SanitizeAndSortDeviceList(
          InputDeviceSettingsController::Get()->GetConnectedPointingSticks()));
}

void InputDeviceSettingsProvider::ObserveMouseSettings(
    mojo::PendingRemote<mojom::MouseSettingsObserver> observer) {
  DCHECK(features::IsInputDeviceSettingsSplitEnabled());
  DCHECK(InputDeviceSettingsController::Get());
  const auto id = mouse_settings_observers_.Add(std::move(observer));
  auto* mouse_settings_observer = mouse_settings_observers_.Get(id);
  mouse_settings_observer->OnMouseListUpdated(SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedMice()));
  mouse_settings_observer->OnMousePoliciesUpdated(
      InputDeviceSettingsController::Get()->GetMousePolicies().Clone());
}

void InputDeviceSettingsProvider::OnKeyboardConnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardDisconnected(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardSettingsUpdated(
    const ::ash::mojom::Keyboard& keyboard) {
  NotifyKeyboardsUpdated();
}

void InputDeviceSettingsProvider::OnKeyboardPoliciesUpdated(
    const ::ash::mojom::KeyboardPolicies& keyboard_policies) {
  for (const auto& observer : keyboard_settings_observers_) {
    observer->OnKeyboardPoliciesUpdated(keyboard_policies.Clone());
  }
}

void InputDeviceSettingsProvider::OnTouchpadConnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadDisconnected(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnTouchpadSettingsUpdated(
    const ::ash::mojom::Touchpad& touchpad) {
  NotifyTouchpadsUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickConnected(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickDisconnected(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnPointingStickSettingsUpdated(
    const ::ash::mojom::PointingStick& pointing_stick) {
  NotifyPointingSticksUpdated();
}

void InputDeviceSettingsProvider::OnMouseConnected(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnMouseDisconnected(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnMouseSettingsUpdated(
    const ::ash::mojom::Mouse& mouse) {
  NotifyMiceUpdated();
}

void InputDeviceSettingsProvider::OnMousePoliciesUpdated(
    const ::ash::mojom::MousePolicies& mouse) {
  for (const auto& observer : mouse_settings_observers_) {
    observer->OnMousePoliciesUpdated(
        InputDeviceSettingsController::Get()->GetMousePolicies().Clone());
  }
}

void InputDeviceSettingsProvider::NotifyKeyboardsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto keyboards = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedKeyboards());
  for (const auto& observer : keyboard_settings_observers_) {
    observer->OnKeyboardListUpdated(mojo::Clone(keyboards));
  }
}

void InputDeviceSettingsProvider::NotifyTouchpadsUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto touchpads = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedTouchpads());
  for (const auto& observer : touchpad_settings_observers_) {
    observer->OnTouchpadListUpdated(mojo::Clone(touchpads));
  }
}

void InputDeviceSettingsProvider::NotifyPointingSticksUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto pointing_sticks = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedPointingSticks());
  for (const auto& observer : pointing_stick_settings_observers_) {
    observer->OnPointingStickListUpdated(mojo::Clone(pointing_sticks));
  }
}

void InputDeviceSettingsProvider::NotifyMiceUpdated() {
  DCHECK(InputDeviceSettingsController::Get());
  auto mice = SanitizeAndSortDeviceList(
      InputDeviceSettingsController::Get()->GetConnectedMice());
  for (const auto& observer : mouse_settings_observers_) {
    observer->OnMouseListUpdated(mojo::Clone(mice));
  }
}

}  // namespace ash::settings
