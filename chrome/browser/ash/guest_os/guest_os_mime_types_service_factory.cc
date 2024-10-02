// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/profiles/profile.h"

namespace guest_os {

// static
GuestOsMimeTypesService* GuestOsMimeTypesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GuestOsMimeTypesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsMimeTypesServiceFactory* GuestOsMimeTypesServiceFactory::GetInstance() {
  static base::NoDestructor<GuestOsMimeTypesServiceFactory> factory;
  return factory.get();
}

GuestOsMimeTypesServiceFactory::GuestOsMimeTypesServiceFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsMimeTypesService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

GuestOsMimeTypesServiceFactory::~GuestOsMimeTypesServiceFactory() = default;

KeyedService* GuestOsMimeTypesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsMimeTypesService(profile);
}

}  // namespace guest_os
