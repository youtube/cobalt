// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/app_session_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"

// static
AppSessionService* AppSessionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppSessionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppSessionService* AppSessionServiceFactory::GetForProfileIfExisting(
    Profile* profile) {
  return static_cast<AppSessionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
AppSessionService* AppSessionServiceFactory::GetForProfileForSessionRestore(
    Profile* profile) {
  AppSessionService* service = GetForProfile(profile);
  if (!service) {
    // If the service has been shutdown, remove the reference to NULL for
    // |profile| so GetForProfile will recreate it.
    GetInstance()->Disassociate(profile);
    service = GetForProfile(profile);
  }
  return service;
}

// static
void AppSessionServiceFactory::ShutdownForProfile(Profile* profile) {
  if (SessionDataServiceFactory::GetForProfile(profile))
    SessionDataServiceFactory::GetForProfile(profile)->StartCleanup();

  // We're about to exit, force creation of the session service if it hasn't
  // been created yet. We do this to ensure session state matches the point in
  // time the user exited.
  AppSessionServiceFactory* factory = GetInstance();
  factory->GetServiceForBrowserContext(profile, true);

  // Shut down and remove the reference to the session service, and replace it
  // with an explicit NULL to prevent it being recreated on the next access.
  factory->BrowserContextShutdown(profile);
  factory->BrowserContextDestroyed(profile);
  factory->Associate(profile, nullptr);
}

AppSessionServiceFactory* AppSessionServiceFactory::GetInstance() {
  return base::Singleton<AppSessionServiceFactory>::get();
}

AppSessionServiceFactory::AppSessionServiceFactory()
    : ProfileKeyedServiceFactory(
          "AppSessionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  // Ensure that session data is cleared before session restore can happen.
  DependsOn(SessionDataServiceFactory::GetInstance());
}

AppSessionServiceFactory::~AppSessionServiceFactory() = default;

KeyedService* AppSessionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  AppSessionService* service = nullptr;
  service = new AppSessionService(static_cast<Profile*>(profile));
  service->ResetFromCurrentBrowsers();
  return service;
}

bool AppSessionServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AppSessionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
