// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

/******** StandaloneBrowserExtensionAppsFactoryForApp ********/

// static
StandaloneBrowserExtensionApps*
StandaloneBrowserExtensionAppsFactoryForApp::GetForProfile(Profile* profile) {
  return static_cast<StandaloneBrowserExtensionApps*>(
      StandaloneBrowserExtensionAppsFactoryForApp::GetInstance()
          ->GetServiceForBrowserContext(profile, true /* create */));
}

// static
StandaloneBrowserExtensionAppsFactoryForApp*
StandaloneBrowserExtensionAppsFactoryForApp::GetInstance() {
  return base::Singleton<StandaloneBrowserExtensionAppsFactoryForApp>::get();
}

// static
void StandaloneBrowserExtensionAppsFactoryForApp::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

StandaloneBrowserExtensionAppsFactoryForApp::
    StandaloneBrowserExtensionAppsFactoryForApp()
    : ProfileKeyedServiceFactory(
          "StandaloneBrowserExtensionAppsForApp",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(AppServiceProxyFactory::GetInstance());
}

KeyedService*
StandaloneBrowserExtensionAppsFactoryForApp::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new StandaloneBrowserExtensionApps(
      AppServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      AppType::kStandaloneBrowserChromeApp);
}

/******** StandaloneBrowserExtensionAppsFactoryForExtension ********/

// static
StandaloneBrowserExtensionApps*
StandaloneBrowserExtensionAppsFactoryForExtension::GetForProfile(
    Profile* profile) {
  return static_cast<StandaloneBrowserExtensionApps*>(
      StandaloneBrowserExtensionAppsFactoryForExtension::GetInstance()
          ->GetServiceForBrowserContext(profile, true /* create */));
}

// static
StandaloneBrowserExtensionAppsFactoryForExtension*
StandaloneBrowserExtensionAppsFactoryForExtension::GetInstance() {
  return base::Singleton<
      StandaloneBrowserExtensionAppsFactoryForExtension>::get();
}

// static
void StandaloneBrowserExtensionAppsFactoryForExtension::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

StandaloneBrowserExtensionAppsFactoryForExtension::
    StandaloneBrowserExtensionAppsFactoryForExtension()
    : ProfileKeyedServiceFactory(
          "StandaloneBrowserExtensionAppsForExtension",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(AppServiceProxyFactory::GetInstance());
}

KeyedService*
StandaloneBrowserExtensionAppsFactoryForExtension::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new StandaloneBrowserExtensionApps(
      AppServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      AppType::kStandaloneBrowserExtension);
}

}  // namespace apps
