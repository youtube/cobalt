// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_DISPLAY_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_DISPLAY_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_handler.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

class ScopedBacklightsForcedOff;

// PowerButtonDisplayController performs display-related tasks (e.g. forcing
// backlights off or disabling the touchscreen) on behalf of
// PowerButtonController and TabletPowerButtonController.
class ASH_EXPORT PowerButtonDisplayController
    : public ScreenBacklightObserver,
      public chromeos::PowerManagerClient::Observer,
      public ui::EventHandler,
      public ui::InputDeviceEventObserver {
 public:
  PowerButtonDisplayController(
      BacklightsForcedOffSetter* backlights_forced_off_setter,
      const base::TickClock* tick_clock);

  PowerButtonDisplayController(const PowerButtonDisplayController&) = delete;
  PowerButtonDisplayController& operator=(const PowerButtonDisplayController&) =
      delete;

  ~PowerButtonDisplayController() override;

  bool IsScreenOn() const;

  base::TimeTicks screen_state_last_changed() const {
    return screen_state_last_changed_;
  }

  // Updates the power manager's backlights-forced-off state and enables or
  // disables the touchscreen. No-op if |backlights_forced_off_| already equals
  // |forced_off|.
  void SetBacklightsForcedOff(bool forced_off);

  // Overridden from ScreenBacklightObserver:
  void OnBacklightsForcedOffChanged(bool forced_off) override;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // Overridden from chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               base::TimeTicks timestamp) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from ui::InputDeviceObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

 private:
  // Saves the most recent timestamp that screen state changed.
  base::TimeTicks screen_state_last_changed_;

  raw_ptr<BacklightsForcedOffSetter, ExperimentalAsh>
      backlights_forced_off_setter_;  // Not owned.

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  // Whether an accessibility alert should be sent when the backlights
  // forced-off state changes.
  bool send_accessibility_alert_on_backlights_forced_off_change_ = false;

  // Time source for performed action times.
  raw_ptr<const base::TickClock, ExperimentalAsh> tick_clock_;  // Not owned.

  // If set, the active request passed to |backlights_forced_off_setter_| in
  // order to force the backlights off.
  std::unique_ptr<ScopedBacklightsForcedOff> backlights_forced_off_;

  base::WeakPtrFactory<PowerButtonDisplayController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_DISPLAY_CONTROLLER_H_
