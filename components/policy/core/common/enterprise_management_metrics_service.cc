// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_metrics_service.h"

#include <string>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

EnterpriseManagementMetricsService::EnterpriseManagementMetricsService(
    ManagementService* platform_management_service,
    ManagementService* browser_management_service,
    PolicyService* policy_service,
    PrefService* local_state,
    PrefService* profile_prefs)
    : platform_management_service_(platform_management_service),
      browser_management_service_(browser_management_service),
      policy_service_(policy_service),
      local_state_(local_state),
      profile_prefs_(profile_prefs) {
  if (local_state_ && platform_management_service_) {
    base::Time last_platform_time =
        local_state_->GetTime(policy_prefs::kLastPlatformManagementLogTime);
    // Checking for future timestamps ensures metric logging resumes immediately
    // if the user adjusts their system clock backward.
    if (base::Time::Now() - last_platform_time >= base::Hours(24) ||
        last_platform_time > base::Time::Now()) {
      CheckAndRecordPlatformMetrics();
      local_state_->SetTime(policy_prefs::kLastPlatformManagementLogTime,
                            base::Time::Now());
    }
  }

  if (profile_prefs_ && browser_management_service_) {
    base::Time last_profile_time =
        profile_prefs_->GetTime(policy_prefs::kLastProfileManagementLogTime);
    // Checking for future timestamps ensures metric logging resumes immediately
    // if the user adjusts their system clock backward.
    if (base::Time::Now() - last_profile_time >= base::Hours(24) ||
        last_profile_time > base::Time::Now()) {
      CheckAndRecordProfileMetrics();
      profile_prefs_->SetTime(policy_prefs::kLastProfileManagementLogTime,
                              base::Time::Now());
    }
  }
}

EnterpriseManagementMetricsService::~EnterpriseManagementMetricsService() =
    default;

// static
void EnterpriseManagementMetricsService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(policy_prefs::kLastPlatformManagementLogTime,
                             base::Time());
}

// static
void EnterpriseManagementMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(policy_prefs::kLastProfileManagementLogTime,
                             base::Time());
}

void EnterpriseManagementMetricsService::CheckAndRecordPlatformMetrics() {
  DCHECK(platform_management_service_);
  if (!platform_management_service_->IsManaged()) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus",
        PlatformManagementStatus::kUnmanaged);
    return;
  }

  if (platform_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus",
        PlatformManagementStatus::kCloudDomain);
  }
  if (platform_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus",
        PlatformManagementStatus::kCloud);
  }
  if (platform_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus",
        PlatformManagementStatus::kDomainLocal);
  }
  if (platform_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::COMPUTER_LOCAL)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus",
        PlatformManagementStatus::kComputerLocal);
  }
}

void EnterpriseManagementMetricsService::CheckAndRecordProfileMetrics() {
  DCHECK(browser_management_service_);
  if (!browser_management_service_->IsManaged()) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.BrowserManagementStatus",
        BrowserManagementStatus::kUnmanaged);
    return;
  }

  if (browser_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.BrowserManagementStatus",
        BrowserManagementStatus::kCloudDomain);
  }
  if (browser_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.BrowserManagementStatus",
        BrowserManagementStatus::kCloud);
  }
  if (browser_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.BrowserManagementStatus",
        BrowserManagementStatus::kDomainLocal);
  }
  if (browser_management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::COMPUTER_LOCAL)) {
    int policy_count = 0;
    if (policy_service_) {
      const auto& policies = policy_service_->GetPolicies(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
      policy_count = static_cast<int>(policies.size());
    }
    BrowserManagementStatus status =
        (policy_count <= 3) ? BrowserManagementStatus::kComputerLocalLe3
                            : BrowserManagementStatus::kComputerLocalGt3;
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.BrowserManagementStatus", status);
  }
}

}  // namespace policy
