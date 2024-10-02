// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/history_aware_site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace site_engagement {

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForProfile(
    content::BrowserContext* browser_context) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /*create=*/false));
}

// static
SiteEngagementServiceFactory* SiteEngagementServiceFactory::GetInstance() {
  return base::Singleton<SiteEngagementServiceFactory>::get();
}

SiteEngagementServiceFactory::SiteEngagementServiceFactory()
    : ProfileKeyedServiceFactory(
          "SiteEngagementService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(prerender::NoStatePrefetchManagerFactory::GetInstance());
  SiteEngagementService::SetServiceProvider(this);
}

SiteEngagementServiceFactory::~SiteEngagementServiceFactory() {
  SiteEngagementService::ClearServiceProvider(this);
}

KeyedService* SiteEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::IMPLICIT_ACCESS);
  return new HistoryAwareSiteEngagementService(context, history);
}

SiteEngagementService* SiteEngagementServiceFactory::GetSiteEngagementService(
    content::BrowserContext* browser_context) {
  return GetForProfile(browser_context);
}

}  // namespace site_engagement
