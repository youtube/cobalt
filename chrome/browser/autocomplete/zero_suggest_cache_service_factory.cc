// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ZeroSuggestCacheService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ZeroSuggestCacheServiceFactory* ZeroSuggestCacheServiceFactory::GetInstance() {
  return base::Singleton<ZeroSuggestCacheServiceFactory>::get();
}

KeyedService* ZeroSuggestCacheServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ZeroSuggestCacheService(
      profile->GetPrefs(), OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

ZeroSuggestCacheServiceFactory::ZeroSuggestCacheServiceFactory()
    : ProfileKeyedServiceFactory(
          "ZeroSuggestCacheServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

ZeroSuggestCacheServiceFactory::~ZeroSuggestCacheServiceFactory() = default;
