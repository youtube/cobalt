// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/renderer_updater_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/signin/identity_manager_factory.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

RendererUpdaterFactory::RendererUpdaterFactory()
    : ProfileKeyedServiceFactory(
          "RendererUpdater",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  DependsOn(BoundSessionCookieRefreshServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}

RendererUpdaterFactory::~RendererUpdaterFactory() {}

// static
RendererUpdaterFactory* RendererUpdaterFactory::GetInstance() {
  return base::Singleton<RendererUpdaterFactory>::get();
}

// static
RendererUpdater* RendererUpdaterFactory::GetForProfile(Profile* profile) {
  return static_cast<RendererUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* RendererUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RendererUpdater(static_cast<Profile*>(context));
}

bool RendererUpdaterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
