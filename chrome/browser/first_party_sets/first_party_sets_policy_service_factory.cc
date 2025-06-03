// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace first_party_sets {

namespace {

BrowserContextKeyedServiceFactory::TestingFactory* GetTestingFactory() {
  static base::NoDestructor<BrowserContextKeyedServiceFactory::TestingFactory>
      instance;
  return instance.get();
}

}  // namespace

// static
FirstPartySetsPolicyService*
FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FirstPartySetsPolicyService*>(
      FirstPartySetsPolicyServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
FirstPartySetsPolicyServiceFactory*
FirstPartySetsPolicyServiceFactory::GetInstance() {
  static base::NoDestructor<FirstPartySetsPolicyServiceFactory> instance;
  return instance.get();
}

void FirstPartySetsPolicyServiceFactory::SetTestingFactoryForTesting(
    TestingFactory test_factory) {
  *GetTestingFactory() = std::move(test_factory);
}

FirstPartySetsPolicyServiceFactory::FirstPartySetsPolicyServiceFactory()
    : ProfileKeyedServiceFactory(
          "FirstPartySetsPolicyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  // TODO(https://crbug.com/1464637): explicitly declare a dependency on
  // HostContentSettingsMapFactory.
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

FirstPartySetsPolicyServiceFactory::~FirstPartySetsPolicyServiceFactory() =
    default;

std::unique_ptr<KeyedService>
FirstPartySetsPolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!GetTestingFactory()->is_null()) {
    return GetTestingFactory()->Run(context);
  }
  return std::make_unique<FirstPartySetsPolicyService>(context);
}

bool FirstPartySetsPolicyServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

void FirstPartySetsPolicyServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kRelatedWebsiteSetsOverrides);
}

}  // namespace first_party_sets
