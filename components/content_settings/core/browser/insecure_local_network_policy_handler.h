// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_INSECURE_LOCAL_NETWORK_POLICY_HANDLER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_INSECURE_LOCAL_NETWORK_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace content_settings {

// Handler for the InsecurePrivateNetworkRequestAllowed policy.
class InsecureLocalNetworkPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  InsecureLocalNetworkPolicyHandler();
  ~InsecureLocalNetworkPolicyHandler() override;

  InsecureLocalNetworkPolicyHandler(const InsecureLocalNetworkPolicyHandler&) =
      delete;
  InsecureLocalNetworkPolicyHandler& operator=(
      const InsecureLocalNetworkPolicyHandler&) = delete;

  // TypeCheckingPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_INSECURE_LOCAL_NETWORK_POLICY_HANDLER_H_
