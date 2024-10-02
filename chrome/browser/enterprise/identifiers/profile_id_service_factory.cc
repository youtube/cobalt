// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"

#include <memory>

#include "chrome/browser/enterprise/identifiers/profile_id_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"

namespace enterprise {

// static
ProfileIdService* ProfileIdServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return static_cast<ProfileIdService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileIdServiceFactory* ProfileIdServiceFactory::GetInstance() {
  return base::Singleton<ProfileIdServiceFactory>::get();
}

ProfileIdServiceFactory::ProfileIdServiceFactory()
    : ProfileKeyedServiceFactory("ProfileIdService",
                                 ProfileSelections::BuildForRegularProfile()) {}

ProfileIdServiceFactory::~ProfileIdServiceFactory() = default;

KeyedService* ProfileIdServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  return new ProfileIdService(std::make_unique<ProfileIdDelegateImpl>(profile),
                              profile->GetPrefs());
}

}  // namespace enterprise
