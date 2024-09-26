// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_factory.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader_impl.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// static
OneGoogleBarService* OneGoogleBarServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OneGoogleBarService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OneGoogleBarServiceFactory* OneGoogleBarServiceFactory::GetInstance() {
  return base::Singleton<OneGoogleBarServiceFactory>::get();
}

OneGoogleBarServiceFactory::OneGoogleBarServiceFactory()
    : ProfileKeyedServiceFactory(
          "OneGoogleBarService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

OneGoogleBarServiceFactory::~OneGoogleBarServiceFactory() = default;

KeyedService* OneGoogleBarServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile).get();
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return new OneGoogleBarService(
      identity_manager,
      std::make_unique<OneGoogleBarLoaderImpl>(
          url_loader_factory, g_browser_process->GetApplicationLocale(),
          AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile) &&
              signin::SettingsAllowSigninCookies(cookie_settings)));
}
