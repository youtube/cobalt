// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEFAULT_SENSORS_SETTING_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEFAULT_SENSORS_SETTING_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class DefaultSensorsSettingPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  DefaultSensorsSettingPolicyHandler();
  DefaultSensorsSettingPolicyHandler(
      const DefaultSensorsSettingPolicyHandler&) = delete;
  DefaultSensorsSettingPolicyHandler& operator=(
      const DefaultSensorsSettingPolicyHandler&) = delete;
  ~DefaultSensorsSettingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEFAULT_SENSORS_SETTING_POLICY_HANDLER_H_
