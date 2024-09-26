// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"

// static
HatsService* HatsServiceFactory::GetForProfile(Profile* profile,
                                               bool create_if_necessary) {
  return static_cast<HatsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
HatsServiceFactory* HatsServiceFactory::GetInstance() {
  return base::Singleton<HatsServiceFactory>::get();
}

HatsServiceFactory::HatsServiceFactory()
    : ProfileKeyedServiceFactory("HatsService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* HatsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new HatsService(profile);
}

HatsServiceFactory::~HatsServiceFactory() = default;
