// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/system/power/power_status.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

class BatteryNotification;
class DualRoleNotification;

// Controller class to manage power/battery notifications.
class ASH_EXPORT PowerNotificationController : public PowerStatus::Observer {
 public:
  enum NotificationState {
    NOTIFICATION_NONE,

    // Low battery charge.
    NOTIFICATION_LOW_POWER,

    // Critically low battery charge.
    NOTIFICATION_CRITICAL,
  };

  explicit PowerNotificationController(
      message_center::MessageCenter* message_center);

  PowerNotificationController(const PowerNotificationController&) = delete;
  PowerNotificationController& operator=(const PowerNotificationController&) =
      delete;

  ~PowerNotificationController() override;

  void NotifyUsbNotificationClosedByUser();

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerNotificationControllerTest,
                           MaybeShowUsbChargerNotification);
  FRIEND_TEST_ALL_PREFIXES(PowerNotificationControllerTest,
                           UpdateNotificationState);
  friend class PowerNotificationControllerTest;

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override;

  // Shows a notification that a low-power USB charger has been connected.
  // Returns true if a notification was shown or explicitly hidden.
  bool MaybeShowUsbChargerNotification();

  // Shows a notification when dual-role devices are connected.
  void MaybeShowDualRoleNotification();

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState();
  bool UpdateNotificationStateForRemainingTime();
  bool UpdateNotificationStateForRemainingPercentage();

  // Time-based notification thresholds when on battery power.
  static constexpr int kCriticalMinutes = 5;
  static constexpr int kLowPowerMinutes = 15;
  static constexpr int kNoWarningMinutes = 30;

  // Percentage-based notification thresholds when using a low-power charger.
  static constexpr int kCriticalPercentage = 5;
  static constexpr int kLowPowerPercentage = 10;
  static constexpr int kNoWarningPercentage = 15;

  static const char kUsbNotificationId[];

  const raw_ptr<message_center::MessageCenter, ExperimentalAsh>
      message_center_;  // Unowned.
  std::unique_ptr<BatteryNotification> battery_notification_;
  std::unique_ptr<DualRoleNotification> dual_role_notification_;
  NotificationState notification_state_ = NOTIFICATION_NONE;

  // Was the battery full the last time OnPowerStatusChanged() was called?
  bool battery_was_full_ = false;

  // Was a USB charger connected the last time OnPowerStatusChanged() was
  // called?
  bool usb_charger_was_connected_ = false;

  // Was line power connected the last time onPowerStatusChanged() was called?
  bool line_power_was_connected_ = false;

  // Has the user already dismissed a low-power notification? Should be set
  // back to false when all power sources are disconnected.
  bool usb_notification_dismissed_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_
