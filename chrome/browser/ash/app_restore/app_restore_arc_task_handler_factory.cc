// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::app_restore {

// static
AppRestoreArcTaskHandler* AppRestoreArcTaskHandlerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppRestoreArcTaskHandler*>(
      AppRestoreArcTaskHandlerFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
AppRestoreArcTaskHandlerFactory*
AppRestoreArcTaskHandlerFactory::GetInstance() {
  return base::Singleton<AppRestoreArcTaskHandlerFactory>::get();
}

AppRestoreArcTaskHandlerFactory::AppRestoreArcTaskHandlerFactory()
    : ProfileKeyedServiceFactory(
          "AppRestoreArcTaskHandler",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

AppRestoreArcTaskHandlerFactory::~AppRestoreArcTaskHandlerFactory() = default;

KeyedService* AppRestoreArcTaskHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!arc::IsArcAllowedForProfile(Profile::FromBrowserContext(context)))
    return nullptr;

  return new AppRestoreArcTaskHandler(Profile::FromBrowserContext(context));
}

}  // namespace ash::app_restore
