// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

// static
ChromeBrowsingDataLifetimeManagerFactory*
ChromeBrowsingDataLifetimeManagerFactory::GetInstance() {
  return base::Singleton<ChromeBrowsingDataLifetimeManagerFactory>::get();
}

// static
ChromeBrowsingDataLifetimeManager*
ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataLifetimeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataLifetimeManagerFactory::
    ChromeBrowsingDataLifetimeManagerFactory()
    : ProfileKeyedServiceFactory(
          "BrowsingDataLifetimeManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(ChromeBrowsingDataRemoverDelegateFactory::GetInstance());
}

ChromeBrowsingDataLifetimeManagerFactory::
    ~ChromeBrowsingDataLifetimeManagerFactory() = default;

KeyedService* ChromeBrowsingDataLifetimeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          browsing_data::features::kEnableBrowsingDataLifetimeManager))
    return nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* profile = Profile::FromBrowserContext(context);
  // This condition still needs to be explicitly stated here despite having
  // ProfileKeyedService logic implemented because `IsGuestSession()` and
  // `IsRegularProfile()` are not yet mutually exclusive in ASH and Lacros.
  // TODO(rsult): remove this condition when `IsGuestSession() is fixed.
  if (profile->IsGuestSession() && !profile->IsOffTheRecord())
    return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return new ChromeBrowsingDataLifetimeManager(context);
}

bool ChromeBrowsingDataLifetimeManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
