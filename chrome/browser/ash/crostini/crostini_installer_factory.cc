// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_installer_factory.h"

#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
crostini::CrostiniInstaller* CrostiniInstallerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<crostini::CrostiniInstaller*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniInstallerFactory* CrostiniInstallerFactory::GetInstance() {
  static base::NoDestructor<CrostiniInstallerFactory> factory;
  return factory.get();
}

CrostiniInstallerFactory::CrostiniInstallerFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniInstallerService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(crostini::CrostiniManagerFactory::GetInstance());
}

CrostiniInstallerFactory::~CrostiniInstallerFactory() = default;

KeyedService* CrostiniInstallerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniInstaller(profile);
}

}  // namespace crostini
