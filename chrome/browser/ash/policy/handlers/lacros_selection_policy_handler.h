// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_SELECTION_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_SELECTION_POLICY_HANDLER_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// The handler for LacrosSelection selection policy which maps string-enum
// policy values to `LacrosSelectionPolicy` enum stored in `PolicyMap`.
class LacrosSelectionPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  LacrosSelectionPolicyHandler();

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  absl::optional<crosapi::browser_util::LacrosSelectionPolicy> GetValue(
      const PolicyMap& policies,
      PolicyErrorMap* errors);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_SELECTION_POLICY_HANDLER_H_
