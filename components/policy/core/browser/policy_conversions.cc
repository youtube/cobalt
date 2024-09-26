// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

using base::Value;

namespace policy {

const webui::LocalizedString kPolicySources[POLICY_SOURCE_COUNT] = {
    {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
    {"commandLine", IDS_POLICY_SOURCE_COMMAND_LINE},
    {"cloud", IDS_POLICY_SOURCE_CLOUD},
    {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
    {"sourceDeviceLocalAccountOverrideDeprecated",
     IDS_POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE},
    {"platform", IDS_POLICY_SOURCE_PLATFORM},
    {"priorityCloud", IDS_POLICY_SOURCE_CLOUD},
    {"merged", IDS_POLICY_SOURCE_MERGED},
    {"cloud_from_ash", IDS_POLICY_SOURCE_CLOUD_FROM_ASH},
    {"restrictedManagedGuestSessionOverride",
     IDS_POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE},
};

const char kIdKey[] = "id";
const char kNameKey[] = "name";
const char kPoliciesKey[] = "policies";
const char kPolicyNamesKey[] = "policyNames";
const char kChromePoliciesId[] = "chrome";
const char kChromePoliciesName[] = "Chrome Policies";

#if !BUILDFLAG(IS_CHROMEOS)
const char kPrecedenceOrderKey[] = "precedenceOrder";
const char kPrecedencePoliciesId[] = "precedence";
const char kPrecedencePoliciesName[] = "Policy Precedence";
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kDeviceLocalAccountPoliciesId[] = "deviceLocalAccountPolicies";
const char kIdentityFieldsId[] = "identityFields";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

PolicyConversions::PolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : client_(std::move(client)) {
  DCHECK(client_.get());
}

PolicyConversions::~PolicyConversions() = default;

PolicyConversions& PolicyConversions::EnableConvertTypes(bool enabled) {
  client_->EnableConvertTypes(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableConvertValues(bool enabled) {
  client_->EnableConvertValues(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceLocalAccountPolicies(
    bool enabled) {
  client_->EnableDeviceLocalAccountPolicies(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableDeviceInfo(bool enabled) {
  client_->EnableDeviceInfo(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnablePrettyPrint(bool enabled) {
  client_->EnablePrettyPrint(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::EnableUserPolicies(bool enabled) {
  client_->EnableUserPolicies(enabled);
  return *this;
}

PolicyConversions& PolicyConversions::SetDropDefaultValues(bool enabled) {
  client_->SetDropDefaultValues(enabled);
  return *this;
}

/**
 * DictionaryPolicyConversions
 */

DictionaryPolicyConversions::DictionaryPolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : PolicyConversions(std::move(client)) {}
DictionaryPolicyConversions::~DictionaryPolicyConversions() = default;

DictionaryPolicyConversions& DictionaryPolicyConversions::EnableConvertTypes(
    bool enabled) {
  PolicyConversions::EnableConvertTypes(enabled);
  return *this;
}

DictionaryPolicyConversions& DictionaryPolicyConversions::EnableConvertValues(
    bool enabled) {
  PolicyConversions::EnableConvertValues(enabled);
  return *this;
}

DictionaryPolicyConversions&
DictionaryPolicyConversions::EnableDeviceLocalAccountPolicies(bool enabled) {
  PolicyConversions::EnableDeviceLocalAccountPolicies(enabled);
  return *this;
}

DictionaryPolicyConversions& DictionaryPolicyConversions::EnableDeviceInfo(
    bool enabled) {
  PolicyConversions::EnableDeviceInfo(enabled);
  return *this;
}

DictionaryPolicyConversions& DictionaryPolicyConversions::EnablePrettyPrint(
    bool enabled) {
  PolicyConversions::EnablePrettyPrint(enabled);
  return *this;
}

DictionaryPolicyConversions& DictionaryPolicyConversions::EnableUserPolicies(
    bool enabled) {
  PolicyConversions::EnableUserPolicies(enabled);
  return *this;
}

DictionaryPolicyConversions& DictionaryPolicyConversions::SetDropDefaultValues(
    bool enabled) {
  PolicyConversions::SetDropDefaultValues(enabled);
  return *this;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
base::Value::Dict DictionaryPolicyConversions::GetExtensionPolicies() {
  base::Value::Dict extension_policies;
  if (client()->HasUserPolicies()) {
    extension_policies.Set("extensionPolicies",
                           GetExtensionPolicies(POLICY_DOMAIN_EXTENSIONS));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extension_policies.Set("loginScreenExtensionPolicies",
                         GetExtensionPolicies(POLICY_DOMAIN_SIGNIN_EXTENSIONS));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return extension_policies;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::string DictionaryPolicyConversions::ToJSON() {
  return client()->ConvertValueToJSON(Value(ToValueDict()));
}

Value::Dict DictionaryPolicyConversions::ToValueDict() {
  Value::Dict all_policies;

  if (client()->HasUserPolicies()) {
    all_policies.Set("chromePolicies", client()->GetChromePolicies());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  all_policies.Merge(GetExtensionPolicies());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  all_policies.Set(kDeviceLocalAccountPoliciesId,
                   GetDeviceLocalAccountPolicies());
  Value::Dict identity_fields = client()->GetIdentityFields();
  if (!identity_fields.empty())
    all_policies.Merge(std::move(identity_fields));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return all_policies;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
Value::Dict DictionaryPolicyConversions::GetDeviceLocalAccountPolicies() {
  Value::List policies = client()->GetDeviceLocalAccountPolicies();
  Value::Dict device_values;
  for (auto&& policy : policies) {
    const std::string* id = policy.GetDict().FindString(kIdKey);
    Value* policies_value = policy.GetDict().Find(kPoliciesKey);
    DCHECK(id);
    DCHECK(policies_value);
    device_values.Set(*id, std::move(*policies_value));
  }
  return device_values;
}
#endif

Value::Dict DictionaryPolicyConversions::GetExtensionPolicies(
    PolicyDomain policy_domain) {
  Value::List policies = client()->GetExtensionPolicies(policy_domain);
  Value::Dict extension_values;
  for (auto&& policy : policies) {
    const std::string* id = policy.GetDict().FindString(kIdKey);
    Value* policies_value = policy.GetDict().Find(kPoliciesKey);
    DCHECK(id);
    DCHECK(policies_value);
    extension_values.Set(*id, std::move(*policies_value));
  }
  return extension_values;
}

/**
 * ChromePolicyConversions
 */

ChromePolicyConversions::ChromePolicyConversions(
    std::unique_ptr<PolicyConversionsClient> client)
    : PolicyConversions(std::move(client)) {}
ChromePolicyConversions::~ChromePolicyConversions() = default;

ChromePolicyConversions& ChromePolicyConversions::EnableConvertTypes(
    bool enabled) {
  PolicyConversions::EnableConvertTypes(enabled);
  return *this;
}

ChromePolicyConversions& ChromePolicyConversions::EnableConvertValues(
    bool enabled) {
  PolicyConversions::EnableConvertValues(enabled);
  return *this;
}

ChromePolicyConversions&
ChromePolicyConversions::EnableDeviceLocalAccountPolicies(bool enabled) {
  PolicyConversions::EnableDeviceLocalAccountPolicies(enabled);
  return *this;
}

ChromePolicyConversions& ChromePolicyConversions::EnableDeviceInfo(
    bool enabled) {
  PolicyConversions::EnableDeviceInfo(enabled);
  return *this;
}

ChromePolicyConversions& ChromePolicyConversions::EnablePrettyPrint(
    bool enabled) {
  PolicyConversions::EnablePrettyPrint(enabled);
  return *this;
}

ChromePolicyConversions& ChromePolicyConversions::EnableUserPolicies(
    bool enabled) {
  PolicyConversions::EnableUserPolicies(enabled);
  return *this;
}

ChromePolicyConversions& ChromePolicyConversions::SetDropDefaultValues(
    bool enabled) {
  PolicyConversions::SetDropDefaultValues(enabled);
  return *this;
}

std::string ChromePolicyConversions::ToJSON() {
  return client()->ConvertValueToJSON(Value(ToValueDict()));
}

Value::Dict ChromePolicyConversions::ToValueDict() {
  Value::Dict all_policies;

  if (client()->HasUserPolicies()) {
    all_policies.Set(kChromePoliciesId, GetChromePolicies());

#if !BUILDFLAG(IS_CHROMEOS)
    // Precedence policies do not apply to Chrome OS, so the Policy Precedence
    // table is not shown in chrome://policy.
    all_policies.Set(kPrecedencePoliciesId, GetPrecedencePolicies());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Value::List local_account_policies =
      client()->GetDeviceLocalAccountPolicies();
  if (!local_account_policies.empty())
    all_policies.Set(kDeviceLocalAccountPoliciesId,
                     std::move(local_account_policies));

  Value::Dict identity_fields = client()->GetIdentityFields();
  if (!identity_fields.empty())
    all_policies.Set(kIdentityFieldsId, std::move(identity_fields));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return all_policies;
}

Value::Dict ChromePolicyConversions::GetChromePolicies() {
  Value::Dict chrome_policies_data;
  chrome_policies_data.Set(kNameKey, kChromePoliciesName);
  Value::Dict chrome_policies = client()->GetChromePolicies();
  chrome_policies_data.Set(kPoliciesKey, std::move(chrome_policies));
  return chrome_policies_data;
}

#if !BUILDFLAG(IS_CHROMEOS)
Value::Dict ChromePolicyConversions::GetPrecedencePolicies() {
  Value::Dict precedence_policies_data;
  precedence_policies_data.Set(kNameKey, kPrecedencePoliciesName);
  precedence_policies_data.Set(kPoliciesKey, client()->GetPrecedencePolicies());
  precedence_policies_data.Set(kPrecedenceOrderKey,
                               client()->GetPrecedenceOrder());
  return precedence_policies_data;
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
