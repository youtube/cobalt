// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_AUTO_LAUNCH_PROTOCOLS_POLICY_HANDLER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_AUTO_LAUNCH_PROTOCOLS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Handles AutoLaunchProtocolsFromOrigins policy.
class AutoLaunchProtocolsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit AutoLaunchProtocolsPolicyHandler(
      const policy::Schema& chrome_schema);

  AutoLaunchProtocolsPolicyHandler(const AutoLaunchProtocolsPolicyHandler&) =
      delete;
  AutoLaunchProtocolsPolicyHandler& operator=(
      const AutoLaunchProtocolsPolicyHandler&) = delete;

  ~AutoLaunchProtocolsPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_AUTO_LAUNCH_PROTOCOLS_POLICY_HANDLER_H_
