// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_controller.h"

#include "ash/constants/ash_features.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

namespace {

#if DCHECK_IS_ON()
// Fake input for notification testing.
constexpr int kFakeNotificationInputForTesting = 8;
#endif  // DCHECK_IS_ON()

}  // namespace

AdaptiveChargingController::AdaptiveChargingController()
    : nudge_controller_(std::make_unique<AdaptiveChargingNudgeController>()),
      notification_controller_(
          std::make_unique<AdaptiveChargingNotificationController>()) {
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
}

AdaptiveChargingController::~AdaptiveChargingController() = default;

bool AdaptiveChargingController::IsAdaptiveChargingSupported() {
  const absl::optional<power_manager::PowerSupplyProperties>&
      power_supply_proto = chromeos::PowerManagerClient::Get()->GetLastStatus();

  return power_supply_proto.has_value() &&
         power_supply_proto->adaptive_charging_supported();
}

void AdaptiveChargingController::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  bool is_on_charger_now = false;
  if (proto.has_external_power()) {
    is_on_charger_now =
        proto.external_power() == power_manager::PowerSupplyProperties::AC;
  }

#if DCHECK_IS_ON()
  if (features::IsAdaptiveChargingForTestingEnabled()) {
    if (!is_on_charger_ && is_on_charger_now) {
      nudge_controller_->ShowNudgeForTesting();  // IN-TEST
      notification_controller_->ShowAdaptiveChargingNotification(
          kFakeNotificationInputForTesting);
    }
    is_on_charger_ = is_on_charger_now;
    return;
  }
#endif  // DCHECK_IS_ON()

  // Nudge and notification should be shown only if heuristic is enabled for
  // this user.
  if (proto.has_adaptive_charging_heuristic_enabled() &&
      !proto.adaptive_charging_heuristic_enabled()) {
    // |is_adaptive_delaying_charge_| is set to false when there is no
    // heuristic enabled. This is to make sure quick settings gets correct
    // information.
    is_adaptive_delaying_charge_ = false;
    notification_controller_->CloseAdaptiveChargingNotification();
    return;
  }

  // Showing educational nudge.
  if (proto.has_adaptive_charging_heuristic_enabled() &&
      proto.adaptive_charging_heuristic_enabled() && !is_on_charger_ &&
      is_on_charger_now)
    nudge_controller_->ShowNudge();

  is_on_charger_ = is_on_charger_now;

  // Return if this change does not contain any adaptive_delaying_charge info.
  if (!proto.has_adaptive_delaying_charge())
    return;

  // We only care about the change in this field.
  if (is_adaptive_delaying_charge_ == proto.adaptive_delaying_charge())
    return;

  is_adaptive_delaying_charge_ = proto.adaptive_delaying_charge();

  // Notification should be shown only if the adaptive charging is actually
  // active.
  if (!is_adaptive_delaying_charge_) {
    notification_controller_->CloseAdaptiveChargingNotification();
    return;
  }

  if (proto.has_battery_time_to_full_sec() &&
      proto.battery_time_to_full_sec() > 0) {
    // Converts time to full from second to hours.
    notification_controller_->ShowAdaptiveChargingNotification(
        static_cast<int>(proto.battery_time_to_full_sec() / 3600));
  } else {
    notification_controller_->ShowAdaptiveChargingNotification();
  }
}

}  // namespace ash
