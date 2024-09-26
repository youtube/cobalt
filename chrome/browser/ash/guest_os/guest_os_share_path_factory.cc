// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"

namespace guest_os {

// static
GuestOsSharePath* GuestOsSharePathFactory::GetForProfile(Profile* profile) {
  return static_cast<GuestOsSharePath*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsSharePathFactory* GuestOsSharePathFactory::GetInstance() {
  static base::NoDestructor<GuestOsSharePathFactory> factory;
  return factory.get();
}

GuestOsSharePathFactory::GuestOsSharePathFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsSharePath",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(crostini::CrostiniManagerFactory::GetInstance());
}

GuestOsSharePathFactory::~GuestOsSharePathFactory() = default;

KeyedService* GuestOsSharePathFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsSharePath(profile);
}

}  // namespace guest_os
