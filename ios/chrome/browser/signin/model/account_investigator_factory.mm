// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_investigator_factory.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service_factory.h"
#import "components/signin/core/browser/account_investigator.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

// static
AccountInvestigatorFactory* AccountInvestigatorFactory::GetInstance() {
  static base::NoDestructor<AccountInvestigatorFactory> instance;
  return instance.get();
}

// static
AccountInvestigator* AccountInvestigatorFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AccountInvestigator>(
      profile, /*create=*/true);
}

AccountInvestigatorFactory::AccountInvestigatorFactory()
    : ProfileKeyedServiceFactoryIOS("AccountInvestigator",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

AccountInvestigatorFactory::~AccountInvestigatorFactory() = default;

std::unique_ptr<KeyedService>
AccountInvestigatorFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  std::unique_ptr<AccountInvestigator> investigator =
      std::make_unique<AccountInvestigator>(
          profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
  investigator->Initialize();
  return std::move(investigator);
}

void AccountInvestigatorFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountInvestigator::RegisterPrefs(registry);
}

}  // namespace ios
