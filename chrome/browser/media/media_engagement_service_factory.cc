// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"

// static
MediaEngagementService* MediaEngagementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaEngagementServiceFactory* MediaEngagementServiceFactory::GetInstance() {
  return base::Singleton<MediaEngagementServiceFactory>::get();
}

MediaEngagementServiceFactory::MediaEngagementServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaEngagementServiceFactory",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

MediaEngagementServiceFactory::~MediaEngagementServiceFactory() {}

KeyedService* MediaEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new MediaEngagementService(Profile::FromBrowserContext(context));
}
