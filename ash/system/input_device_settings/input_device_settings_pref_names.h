// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_

namespace ash::prefs {
// Prefs which contain lists of observed devices for a few milestones before
// per-device settings are enabled.
constexpr char kKeyboardObservedDevicesPref[] =
    "settings.keyboard.observed_devices";
constexpr char kMouseObservedDevicesPref[] = "settings.mouse.observed_devices";
constexpr char kPointingStickObservedDevicesPref[] =
    "settings.pointing_stick.observed_devices";
constexpr char kTouchpadObservedDevicesPref[] =
    "settings.touchpad.observed_devices";

// Prefs which contain login screen settings for connected internal devices.
constexpr char kKeyboardLoginScreenInternalSettingsPref[] =
    "settings.keyboard.internal";
constexpr char kMouseLoginScreenInternalSettingsPref[] =
    "settings.mouse.internal";
constexpr char kPointingStickLoginScreenInternalSettingsPref[] =
    "settings.pointing_stick.internal";
constexpr char kTouchpadLoginScreenInternalSettingsPref[] =
    "settings.touchpad.internal";

// Prefs which contain login screen settings for connected external devices.
constexpr char kKeyboardLoginScreenExternalSettingsPref[] =
    "settings.keyboard.external";
constexpr char kMouseLoginScreenExternalSettingsPref[] =
    "settings.mouse.external";
constexpr char kPointingStickLoginScreenExternalSettingsPref[] =
    "settings.pointing_stick.external";
constexpr char kTouchpadLoginScreenExternalSettingsPref[] =
    "settings.touchpad.external";

// Prefs which contain dictionaries of settings for each connected device.
constexpr char kKeyboardDeviceSettingsDictPref[] = "settings.keyboard.devices";
constexpr char kMouseDeviceSettingsDictPref[] = "settings.mouse.devices";
constexpr char kPointingStickDeviceSettingsDictPref[] =
    "settings.pointing_stick.devices";
constexpr char kTouchpadDeviceSettingsDictPref[] = "settings.touchpad.devices";

// Keyboard settings dictionary keys.
constexpr char kKeyboardSettingAutoRepeatDelay[] = "auto_repeat_delay";
constexpr char kKeyboardSettingAutoRepeatEnabled[] = "auto_repeat_enabled";
constexpr char kKeyboardSettingAutoRepeatInterval[] = "auto_repeat_interval";
constexpr char kKeyboardSettingModifierRemappings[] = "modifier_remappings";
constexpr char kKeyboardSettingSuppressMetaFKeyRewrites[] =
    "suppress_meta_fkey_rewrites";
constexpr char kKeyboardSettingTopRowAreFKeys[] = "top_row_are_fkeys";

// Mouse settings dictionary keys.
constexpr char kMouseSettingSwapRight[] = "swap_right";
constexpr char kMouseSettingSensitivity[] = "sensitivity";
constexpr char kMouseSettingReverseScrolling[] = "reverse_scrolling";
constexpr char kMouseSettingAccelerationEnabled[] = "acceleration_enabled";
constexpr char kMouseSettingScrollSensitivity[] = "scroll_sensitivity";
constexpr char kMouseSettingScrollAcceleration[] = "scroll_acceleration";

// Touchpad settings dictionary keys.
constexpr char kTouchpadSettingSensitivity[] = "sensitivity";
constexpr char kTouchpadSettingReverseScrolling[] = "reverse_scrolling";
constexpr char kTouchpadSettingAccelerationEnabled[] = "acceleration_enabled";
constexpr char kTouchpadSettingScrollSensitivity[] = "scroll_sensitivity";
constexpr char kTouchpadSettingScrollAcceleration[] = "scroll_acceleration";
constexpr char kTouchpadSettingTapToClickEnabled[] = "tap_to_click_enabled";
constexpr char kTouchpadSettingThreeFingerClickEnabled[] =
    "three_finger_click_enabled";
constexpr char kTouchpadSettingTapDraggingEnabled[] = "tap_dragging_enabled";
constexpr char kTouchpadSettingHapticSensitivity[] = "haptic_sensitivity";
constexpr char kTouchpadSettingHapticEnabled[] = "haptic_enabled";

// Pointing stick settings dictionary keys.
constexpr char kPointingStickSettingSensitivity[] = "sensitivity";
constexpr char kPointingStickSettingSwapRight[] = "swap_right";
constexpr char kPointingStickSettingAcceleration[] = "acceleration";

}  // namespace ash::prefs

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
