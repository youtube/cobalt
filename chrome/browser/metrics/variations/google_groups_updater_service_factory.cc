// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/google_groups_updater_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/service/google_groups_updater_service.h"

// static
GoogleGroupsUpdaterServiceFactory*
GoogleGroupsUpdaterServiceFactory::GetInstance() {
  static base::NoDestructor<GoogleGroupsUpdaterServiceFactory> instance;
  return instance.get();
}

// static
GoogleGroupsUpdaterService*
GoogleGroupsUpdaterServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<GoogleGroupsUpdaterService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

GoogleGroupsUpdaterServiceFactory::GoogleGroupsUpdaterServiceFactory()
    : ProfileKeyedServiceFactory(
          "GoogleGroupsUpdaterService",
          // We only want instances of this service corresponding to regular
          // profiles, as those are the only ones that can have sync data to
          // copy from.
          // In the case of Incognito, the OTR profile will not have the service
          // created however the owning regular profile will be loaded and have
          // the service created.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

std::unique_ptr<KeyedService>
GoogleGroupsUpdaterServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GoogleGroupsUpdaterService>(
      *g_browser_process->local_state(),
      // Profile paths are guaranteed to be UTF8-encoded.
      profile->GetPath().BaseName().AsUTF8Unsafe(), *profile->GetPrefs());
}

bool GoogleGroupsUpdaterServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

void GoogleGroupsUpdaterServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  GoogleGroupsUpdaterService::RegisterProfilePrefs(registry);
}
