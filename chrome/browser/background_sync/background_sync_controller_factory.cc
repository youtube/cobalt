// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_factory.h"

#include "chrome/browser/background_sync/background_sync_delegate_impl.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/background_sync/background_sync_controller_impl.h"

// static
BackgroundSyncControllerImpl* BackgroundSyncControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundSyncControllerImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BackgroundSyncControllerFactory*
BackgroundSyncControllerFactory::GetInstance() {
  return base::Singleton<BackgroundSyncControllerFactory>::get();
}

BackgroundSyncControllerFactory::BackgroundSyncControllerFactory()
    : ProfileKeyedServiceFactory(
          "BackgroundSyncService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

BackgroundSyncControllerFactory::~BackgroundSyncControllerFactory() = default;

KeyedService* BackgroundSyncControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BackgroundSyncControllerImpl(
      context, std::make_unique<BackgroundSyncDelegateImpl>(
                   Profile::FromBrowserContext(context)));
}
