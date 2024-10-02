// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_policy_handler.h"

#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

HttpsOnlyModePolicyHandler::HttpsOnlyModePolicyHandler(
    const char* const pref_name)
    : TypeCheckingPolicyHandler(key::kHttpsOnlyMode, base::Value::Type::STRING),
      pref_name_(pref_name) {}

HttpsOnlyModePolicyHandler::~HttpsOnlyModePolicyHandler() = default;

void HttpsOnlyModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kHttpsOnlyMode, base::Value::Type::STRING);
  // The policy supports force-disabling the HTTPS-First Mode pref
  // ("disallowed"), force-enabling the pref ("force_enabled"), or allowing the
  // user to choose (no policy or setting it to "allowed").
  //
  // For backwards compatibility, we're stuck mapping these string-enum values
  // to the boolean pref states, rather than being able to do a simple
  // policy-pref mapping.
  if (value && value->GetString() == "disallowed") {
    prefs->SetBoolean(pref_name_, false);
  } else if (value && value->GetString() == "force_enabled") {
    prefs->SetBoolean(pref_name_, true);
  }
}

}  // namespace policy
