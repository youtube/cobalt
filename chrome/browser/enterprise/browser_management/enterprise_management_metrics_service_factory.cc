// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/enterprise_management_metrics_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/policy/core/common/enterprise_management_metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace policy {

// static
EnterpriseManagementMetricsServiceFactory*
EnterpriseManagementMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseManagementMetricsServiceFactory> instance;
  return instance.get();
}

// static
EnterpriseManagementMetricsService*
EnterpriseManagementMetricsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EnterpriseManagementMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// This service is excluded from Incognito and Guest profiles because ephemeral
// preference stores would reset the 24-hour UMA throttling timer
// on every window opening, leading to redundant metric logs.
EnterpriseManagementMetricsServiceFactory::
    EnterpriseManagementMetricsServiceFactory()
    : ProfileKeyedServiceFactory("EnterpriseManagementMetricsService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(ManagementServiceFactory::GetInstance());
}

EnterpriseManagementMetricsServiceFactory::
    ~EnterpriseManagementMetricsServiceFactory() = default;

bool EnterpriseManagementMetricsServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EnterpriseManagementMetricsServiceFactory::ServiceIsNULLWhileTesting()
    const {
  // Don't create the service for TestingProfile used in unit_tests because
  // EnterpriseManagementMetricsService uses g_browser_process->local_state(),
  // which is uninitialized or null in many unit_tests cases.
  return true;
}

void EnterpriseManagementMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  EnterpriseManagementMetricsService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService> EnterpriseManagementMetricsServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<EnterpriseManagementMetricsService>(
      ManagementServiceFactory::GetForPlatform(),
      ManagementServiceFactory::GetForProfile(profile),
      profile->GetProfilePolicyConnector()->policy_service(),
      g_browser_process->local_state(), profile->GetPrefs());
}

}  // namespace policy
