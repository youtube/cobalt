// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_hats_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_hats_manager.h"

namespace ash::settings {

// static
OsSettingsHatsManager* OsSettingsHatsManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OsSettingsHatsManager*>(
      OsSettingsHatsManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
OsSettingsHatsManagerFactory* OsSettingsHatsManagerFactory::GetInstance() {
  static base::NoDestructor<OsSettingsHatsManagerFactory> factory;
  return factory.get();
}

OsSettingsHatsManagerFactory::OsSettingsHatsManagerFactory()
    : ProfileKeyedServiceFactory(
          "OsSettingsHatsManager",
          ProfileSelections::BuildForRegularAndIncognito()) {}

OsSettingsHatsManagerFactory::~OsSettingsHatsManagerFactory() = default;

KeyedService* OsSettingsHatsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OsSettingsHatsManager(context);
}

bool OsSettingsHatsManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* OsSettingsHatsManagerFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactory testing_factory) {
  KeyedService* mock_settings_hats_manager =
      ProfileKeyedServiceFactory::SetTestingFactoryAndUse(
          context, std::move(testing_factory));

  return mock_settings_hats_manager;
}

}  // namespace ash::settings
