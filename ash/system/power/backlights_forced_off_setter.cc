// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/backlights_forced_off_setter.h"

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/touch/touch_devices_controller.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "ui/display/manager/touch_device_manager.h"

namespace ash {

BacklightsForcedOffSetter::BacklightsForcedOffSetter() {
  InitDisableTouchscreenWhileScreenOff();

  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
  GetInitialBacklightsForcedOff();
}

BacklightsForcedOffSetter::~BacklightsForcedOffSetter() {
  if (active_backlights_forced_off_count_ > 0) {
    chromeos::PowerManagerClient::Get()->SetBacklightsForcedOff(false);
  }
}

void BacklightsForcedOffSetter::AddObserver(ScreenBacklightObserver* observer) {
  observers_.AddObserver(observer);
}

void BacklightsForcedOffSetter::RemoveObserver(
    ScreenBacklightObserver* observer) {
  observers_.RemoveObserver(observer);
}

ScreenBacklightState BacklightsForcedOffSetter::GetScreenBacklightState()
    const {
  return screen_backlight_state_;
}

std::unique_ptr<ScopedBacklightsForcedOff>
BacklightsForcedOffSetter::ForceBacklightsOff() {
  auto scoped_backlights_forced_off =
      std::make_unique<ScopedBacklightsForcedOff>(base::BindOnce(
          &BacklightsForcedOffSetter::OnScopedBacklightsForcedOffDestroyed,
          weak_ptr_factory_.GetWeakPtr()));
  ++active_backlights_forced_off_count_;
  SetBacklightsForcedOff(true);
  return scoped_backlights_forced_off;
}

void BacklightsForcedOffSetter::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  const bool user_initiated =
      change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;

  const ScreenBacklightState old_state = screen_backlight_state_;
  if (change.percent() > 0.0)
    screen_backlight_state_ = ScreenBacklightState::ON;
  else
    screen_backlight_state_ = user_initiated ? ScreenBacklightState::OFF
                                             : ScreenBacklightState::OFF_AUTO;

  if (screen_backlight_state_ != old_state) {
    for (auto& observer : observers_)
      observer.OnScreenBacklightStateChanged(screen_backlight_state_);
  }

  // Disable the touchscreen when the screen is turned off due to inactivity:
  // https://crbug.com/743291
  if ((screen_backlight_state_ == ScreenBacklightState::OFF_AUTO) !=
          (old_state == ScreenBacklightState::OFF_AUTO) &&
      disable_touchscreen_while_screen_off_) {
    UpdateTouchscreenStatus();
  }
}

void BacklightsForcedOffSetter::PowerManagerRestarted() {
  if (backlights_forced_off_.has_value()) {
    chromeos::PowerManagerClient::Get()->SetBacklightsForcedOff(
        backlights_forced_off_.value());
  } else {
    GetInitialBacklightsForcedOff();
  }
}

void BacklightsForcedOffSetter::ResetForTest() {
  if (active_backlights_forced_off_count_ > 0) {
    chromeos::PowerManagerClient::Get()->SetBacklightsForcedOff(false);
  }

  // Cancel all backlights forced off requests.
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_backlights_forced_off_count_ = 0;

  InitDisableTouchscreenWhileScreenOff();

  backlights_forced_off_.reset();
  GetInitialBacklightsForcedOff();
}

void BacklightsForcedOffSetter::InitDisableTouchscreenWhileScreenOff() {
  disable_touchscreen_while_screen_off_ =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTouchscreenUsableWhileScreenOff);
}

void BacklightsForcedOffSetter::GetInitialBacklightsForcedOff() {
  chromeos::PowerManagerClient::Get()->GetBacklightsForcedOff(base::BindOnce(
      &BacklightsForcedOffSetter::OnGotInitialBacklightsForcedOff,
      weak_ptr_factory_.GetWeakPtr()));
}

void BacklightsForcedOffSetter::OnGotInitialBacklightsForcedOff(
    absl::optional<bool> is_forced_off) {
  if (backlights_forced_off_.has_value() || !is_forced_off.has_value())
    return;

  backlights_forced_off_ = is_forced_off;

  UpdateTouchscreenStatus();

  for (auto& observer : observers_)
    observer.OnBacklightsForcedOffChanged(backlights_forced_off_.value());
}

void BacklightsForcedOffSetter::OnScopedBacklightsForcedOffDestroyed() {
  DCHECK_GT(active_backlights_forced_off_count_, 0);

  --active_backlights_forced_off_count_;
  SetBacklightsForcedOff(active_backlights_forced_off_count_ > 0);
}

void BacklightsForcedOffSetter::SetBacklightsForcedOff(bool forced_off) {
  if (backlights_forced_off_.has_value() &&
      backlights_forced_off_.value() == forced_off) {
    return;
  }

  backlights_forced_off_ = forced_off;
  chromeos::PowerManagerClient::Get()->SetBacklightsForcedOff(forced_off);

  UpdateTouchscreenStatus();

  for (auto& observer : observers_)
    observer.OnBacklightsForcedOffChanged(backlights_forced_off_.value());
}

void BacklightsForcedOffSetter::UpdateTouchscreenStatus() {
  // If there is an external touch display connected to the device, then we want
  // to allow users to wake up the device using this external touch device. The
  // kernel blocks wake up events from internal input devices when the screen is
  // off or is in the suspended state.
  // See https://crbug/797411 for more details.
  const bool disable_touchscreen =
      backlights_forced_off_.value_or(false) ||
      (screen_backlight_state_ == ScreenBacklightState::OFF_AUTO &&
       disable_touchscreen_while_screen_off_ &&
       !display::HasExternalTouchscreenDevice());
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      !disable_touchscreen, TouchDeviceEnabledSource::GLOBAL);
}

}  // namespace ash
