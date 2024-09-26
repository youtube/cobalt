// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"

namespace ash {

// static
ApkWebAppService* ApkWebAppServiceFactory::GetForProfile(Profile* profile) {
  // ApkWebAppService is not supported if web apps aren't available.
  if (!web_app::AreWebAppsEnabled(profile))
    return nullptr;

  return static_cast<ApkWebAppService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ApkWebAppServiceFactory* ApkWebAppServiceFactory::GetInstance() {
  return base::Singleton<ApkWebAppServiceFactory>::get();
}

ApkWebAppServiceFactory::ApkWebAppServiceFactory()
    : ProfileKeyedServiceFactory(
          "ApkWebAppService",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

ApkWebAppServiceFactory::~ApkWebAppServiceFactory() {}

KeyedService* ApkWebAppServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  if (!arc::IsArcAllowedForProfile(profile))
    return nullptr;

  return new ApkWebAppService(profile, /*test_delegate=*/nullptr);
}

}  // namespace ash
