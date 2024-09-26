// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/real_time_url_lookup_service_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#import "components/safe_browsing/core/browser/sync/sync_utils.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/utils.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/safe_browsing/user_population_helper.h"
#import "ios/chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
safe_browsing::RealTimeUrlLookupService*
RealTimeUrlLookupServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<safe_browsing::RealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
RealTimeUrlLookupServiceFactory*
RealTimeUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<RealTimeUrlLookupServiceFactory> instance;
  return instance.get();
}

RealTimeUrlLookupServiceFactory::RealTimeUrlLookupServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RealTimeUrlLookupService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(VerdictCacheManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
RealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::RealTimeUrlLookupService>(
      safe_browsing_service->GetURLLoaderFactory(),
      VerdictCacheManagerFactory::GetForBrowserState(chrome_browser_state),
      base::BindRepeating(&GetUserPopulationForBrowserState,
                          chrome_browser_state),
      chrome_browser_state->GetPrefs(),
      std::make_unique<safe_browsing::SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)),
      base::BindRepeating(
          &safe_browsing::SyncUtils::
              AreSigninAndSyncSetUpForSafeBrowsingTokenFetches,
          SyncServiceFactory::GetForBrowserState(chrome_browser_state),
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)),
      chrome_browser_state->IsOffTheRecord(),
      GetApplicationContext()->GetVariationsService(),
      // Referrer chain provider is currently not available on iOS. Once it
      // is implemented, inject it to enable referrer chain in real time
      // requests.
      /*referrer_chain_provider=*/nullptr);
}
