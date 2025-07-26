// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_service_base.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

ConnectorsServiceBase::DmToken::DmToken(const std::string& value,
                                        policy::PolicyScope scope)
    : value(value), scope(scope) {}
ConnectorsServiceBase::DmToken::DmToken(DmToken&&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    DmToken&&) = default;
ConnectorsServiceBase::DmToken::DmToken(const DmToken&) = default;
ConnectorsServiceBase::DmToken& ConnectorsServiceBase::DmToken::operator=(
    const DmToken&) = default;
ConnectorsServiceBase::DmToken::~DmToken() = default;

base::expected<std::string,
               ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason>
ConnectorsServiceBase::GetDMTokenForRealTimeUrlCheck() const {
  if (!ConnectorsEnabled()) {
    return base::unexpected(
        NoDMTokenForRealTimeUrlCheckReason::kConnectorsDisabled);
  }

  if (GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode) ==
      REAL_TIME_CHECK_DISABLED) {
    return base::unexpected(
        NoDMTokenForRealTimeUrlCheckReason::kPolicyDisabled);
  }

  std::optional<DmToken> dm_token =
      GetDmToken(kEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value()) {
    return dm_token.value().value;
  }
  return base::unexpected(NoDMTokenForRealTimeUrlCheckReason::kNoDmToken);
}

EnterpriseRealTimeUrlCheckMode
ConnectorsServiceBase::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(kEnterpriseRealTimeUrlCheckScope).has_value()) {
    return REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<EnterpriseRealTimeUrlCheckMode>(
      GetPrefs()->GetInteger(kEnterpriseRealTimeUrlCheckMode));
}

std::vector<std::string>
ConnectorsServiceBase::GetReportingServiceProviderNames() {
  if (!ConnectorsEnabled()) {
    return {};
  }

  if (!GetDmToken(kOnSecurityEventScopePref).has_value()) {
    return {};
  }

  return GetConnectorsManagerBase()->GetReportingServiceProviderNames();
}

std::optional<ReportingSettings> ConnectorsServiceBase::GetReportingSettings() {
  if (!ConnectorsEnabled()) {
    return std::nullopt;
  }

  std::optional<ReportingSettings> settings =
      GetConnectorsManagerBase()->GetReportingSettings();
  if (!settings.has_value()) {
    return std::nullopt;
  }

  std::optional<DmToken> dm_token = GetDmToken(kOnSecurityEventScopePref);
  if (!dm_token.has_value()) {
    return std::nullopt;
  }

  settings.value().dm_token = dm_token.value().value;
  settings.value().per_profile =
      dm_token.value().scope == policy::POLICY_SCOPE_USER;

  return settings;
}

#if !BUILDFLAG(IS_CHROMEOS)
std::optional<std::string> ConnectorsServiceBase::GetProfileDmToken() const {
  policy::CloudPolicyManager* policy_manager =
      GetManagedUserCloudPolicyManager();
  if (policy_manager && policy_manager->core() &&
      policy_manager->core()->store() &&
      policy_manager->core()->store()->has_policy() &&
      policy_manager->core()->store()->policy()->has_request_token()) {
    return policy_manager->core()->store()->policy()->request_token();
  }

  return std::nullopt;
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace enterprise_connectors
