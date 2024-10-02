// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyBlocklistService::PolicyBlocklistService(
    web::BrowserState* browser_state,
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

PolicyBlocklistService::~PolicyBlocklistService() = default;

policy::URLBlocklist::URLBlocklistState
PolicyBlocklistService::GetURLBlocklistState(const GURL& url) const {
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

// static
PolicyBlocklistServiceFactory* PolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<PolicyBlocklistServiceFactory> instance;
  return instance.get();
}

// static
PolicyBlocklistService* PolicyBlocklistServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

PolicyBlocklistServiceFactory::PolicyBlocklistServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PolicyBlocklist",
          BrowserStateDependencyManager::GetInstance()) {}

PolicyBlocklistServiceFactory::~PolicyBlocklistServiceFactory() = default;

std::unique_ptr<KeyedService>
PolicyBlocklistServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  PrefService* prefs =
      ChromeBrowserState::FromBrowserState(browser_state)->GetPrefs();
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      prefs, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return std::make_unique<PolicyBlocklistService>(
      browser_state, std::move(url_blocklist_manager));
}

web::BrowserState* PolicyBlocklistServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  // Create the service for both normal and incognito browser states.
  return browser_state;
}
