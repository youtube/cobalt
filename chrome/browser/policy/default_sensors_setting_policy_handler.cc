// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_sensors_setting_policy_handler.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "services/device/public/cpp/device_features.h"

namespace policy {

DefaultSensorsSettingPolicyHandler::DefaultSensorsSettingPolicyHandler()
    : IntRangePolicyHandlerBase(key::kDefaultSensorsSetting, 1, 3, false) {}

DefaultSensorsSettingPolicyHandler::~DefaultSensorsSettingPolicyHandler() =
    default;

void DefaultSensorsSettingPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)) {
    if (value_in_range == 3) {
      bool tri_state_enabled =
          base::FeatureList::GetInstance() &&
          base::FeatureList::IsEnabled(
              features::kSensorsAllowAskBlockPermissionModel);

      if (!tri_state_enabled) {
        // Fallback to ALLOW (1) to maintain the historical default behavior
        // for older clients/configurations when the 'Ask' state is not
        // available.
        value_in_range = CONTENT_SETTING_ALLOW;
      }
    }
    prefs->SetInteger(prefs::kManagedDefaultSensorsSetting, value_in_range);
  }
}

}  // namespace policy
