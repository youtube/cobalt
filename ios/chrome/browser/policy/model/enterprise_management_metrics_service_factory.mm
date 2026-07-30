// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/enterprise_management_metrics_service_factory.h"

#import "components/policy/core/common/enterprise_management_metrics_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace policy {

// static
EnterpriseManagementMetricsServiceFactory*
EnterpriseManagementMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseManagementMetricsServiceFactory> instance;
  return instance.get();
}

// static
EnterpriseManagementMetricsService*
EnterpriseManagementMetricsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<EnterpriseManagementMetricsService>(
          profile, /*create=*/true);
}

// This service is excluded from Incognito profiles because ephemeral
// preference stores would reset the 24-hour UMA throttling timer on every
// window opening, leading to redundant metric logs.
EnterpriseManagementMetricsServiceFactory::
    EnterpriseManagementMetricsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("EnterpriseManagementMetricsService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BrowserManagementServiceFactory::GetInstance());
}

EnterpriseManagementMetricsServiceFactory::
    ~EnterpriseManagementMetricsServiceFactory() = default;

void EnterpriseManagementMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  EnterpriseManagementMetricsService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
EnterpriseManagementMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<EnterpriseManagementMetricsService>(
      BrowserManagementServiceFactory::GetForPlatform(),
      BrowserManagementServiceFactory::GetForProfile(profile),
      profile->GetPolicyConnector()->GetPolicyService(),
      GetApplicationContext()->GetLocalState(), profile->GetPrefs());
}

}  // namespace policy
