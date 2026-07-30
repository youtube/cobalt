// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ISOLATE_ORIGINS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_ISOLATE_ORIGINS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the IsolateOrigins-related policies.
//
// This handler manages:
// - `IsolateOrigins` (desktop)
// - `IsolateOriginsShortlist` (for low-end devices)
// - `IsolateOriginsAndroid` (deprecated mobile-only policy)
//
// At runtime, it maps these policies to the
// `prefs::kIsolateOrigins` preference by checking the device's physical
// memory. High-end devices will respect `IsolateOrigins`, while low-end
// devices will respect `IsolateOriginsShortlist` (falling back to the
// deprecated `IsolateOriginsAndroid` if the shortlist is not set).
class IsolateOriginsPolicyHandler : public ConfigurationPolicyHandler {
 public:
  IsolateOriginsPolicyHandler();
  IsolateOriginsPolicyHandler(const IsolateOriginsPolicyHandler&) = delete;
  IsolateOriginsPolicyHandler& operator=(const IsolateOriginsPolicyHandler&) =
      delete;
  ~IsolateOriginsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ISOLATE_ORIGINS_POLICY_HANDLER_H_
