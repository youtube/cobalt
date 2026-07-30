// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

DeviceWeeklyScheduledSuspendPolicyHandler::
    DeviceWeeklyScheduledSuspendPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kDeviceWeeklyScheduledSuspend,
          chrome_schema.GetKnownProperty(key::kDeviceWeeklyScheduledSuspend),
          SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

DeviceWeeklyScheduledSuspendPolicyHandler::
    ~DeviceWeeklyScheduledSuspendPolicyHandler() = default;

// static
void DeviceWeeklyScheduledSuspendPolicyHandler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(ash::prefs::kDeviceWeeklyScheduledSuspend);
  registry->RegisterIntegerPref(
      ash::prefs::kDeviceWeeklyScheduledResuspendDelayMs, /*default_value=*/-1);
}

// ConfigurationPolicyHandler methods:
bool DeviceWeeklyScheduledSuspendPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* device_weekly_scheduled_suspend =
      policies.GetValueUnsafe(key::kDeviceWeeklyScheduledSuspend);
  if (!device_weekly_scheduled_suspend) {
    return true;
  }

  return true;
}

void DeviceWeeklyScheduledSuspendPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const policy::PolicyMap::Entry* policy =
      policies.Get(key::kDeviceWeeklyScheduledSuspend);
  if (policy && policy->value_unsafe()) {
    prefs->SetValue(ash::prefs::kDeviceWeeklyScheduledSuspend,
                    policy->value_unsafe()->Clone());
  }
}

}  // namespace policy
