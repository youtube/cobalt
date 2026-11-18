// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/isolate_origins_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

bool ValidatePolicyType(const PolicyMap& policies,
                        const char* policy_name,
                        base::Value::Type expected_type,
                        PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValueUnsafe(policy_name);
  if (value && value->type() != expected_type) {
    if (errors) {
      errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(expected_type));
    }
    return false;
  }
  return true;
}

}  // namespace

IsolateOriginsPolicyHandler::IsolateOriginsPolicyHandler() = default;
IsolateOriginsPolicyHandler::~IsolateOriginsPolicyHandler() = default;

bool IsolateOriginsPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                      PolicyErrorMap* errors) {
#if BUILDFLAG(IS_ANDROID)
  const char* isolate_origins_policy = key::kIsolateOriginsAndroid;
#else
  const char* isolate_origins_policy = key::kIsolateOrigins;
#endif

  return ValidatePolicyType(policies, isolate_origins_policy,
                            base::Value::Type::STRING, errors);
}

void IsolateOriginsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  // TODO(crbug.com/466111831): Handling will be added to apply the
  // IsolateOrigins policy for Android Desktop rather than the
  // IsolateOriginsAndroid policy as is currently applied.
#if BUILDFLAG(IS_ANDROID)
  const base::Value* isolate_origins_value =
      policies.GetValue(key::kIsolateOriginsAndroid, base::Value::Type::STRING);
#else
  const base::Value* isolate_origins_value =
      policies.GetValue(key::kIsolateOrigins, base::Value::Type::STRING);
#endif

  if (isolate_origins_value) {
    prefs->SetString(prefs::kIsolateOrigins,
                     isolate_origins_value->GetString());
  }
}

}  // namespace policy
