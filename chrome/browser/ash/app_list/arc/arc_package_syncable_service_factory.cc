// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service_factory.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace arc {

// static
ArcPackageSyncableService*
ArcPackageSyncableServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcPackageSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcPackageSyncableServiceFactory*
ArcPackageSyncableServiceFactory::GetInstance() {
  return base::Singleton<ArcPackageSyncableServiceFactory>::get();
}

ArcPackageSyncableServiceFactory::ArcPackageSyncableServiceFactory()
    : ProfileKeyedServiceFactory(
          "ArcPackageSyncableService",
          // This matches the logic in ExtensionSyncServiceFactory, which uses
          // the original browser context.
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
}

ArcPackageSyncableServiceFactory::~ArcPackageSyncableServiceFactory() {}

KeyedService* ArcPackageSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(profile);

  return ArcPackageSyncableService::Create(profile,
                                           ArcAppListPrefs::Get(profile));
}

}  // namespace arc
