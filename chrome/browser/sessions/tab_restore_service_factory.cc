// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_factory.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "components/sessions/core/tab_restore_service_impl.h"

namespace {

std::unique_ptr<KeyedService> BuildTemplateService(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(!profile->IsOffTheRecord());
  auto client = std::make_unique<ChromeTabRestoreServiceClient>(profile);
  return std::make_unique<sessions::TabRestoreServiceImpl>(
      std::move(client), profile->GetPrefs(), nullptr);
}

}  // namespace

// static
sessions::TabRestoreService* TabRestoreServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
sessions::TabRestoreService* TabRestoreServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
  return static_cast<sessions::TabRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
void TabRestoreServiceFactory::ResetForProfile(Profile* profile) {
  TabRestoreServiceFactory* factory = GetInstance();
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
}

TabRestoreServiceFactory* TabRestoreServiceFactory::GetInstance() {
  return base::Singleton<TabRestoreServiceFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
TabRestoreServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTemplateService);
}

TabRestoreServiceFactory::TabRestoreServiceFactory()
    : ProfileKeyedServiceFactory(
          "sessions::TabRestoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

TabRestoreServiceFactory::~TabRestoreServiceFactory() = default;

bool TabRestoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* TabRestoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return BuildTemplateService(browser_context).release();
}
