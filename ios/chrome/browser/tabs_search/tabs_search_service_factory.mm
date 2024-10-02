// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs_search/tabs_search_service_factory.h"

#import "base/check.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/tabs_search/tabs_search_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

history::WebHistoryService* WebHistoryServiceGetter(
    base::WeakPtr<ChromeBrowserState> weak_browser_state) {
  DCHECK(weak_browser_state.get())
      << "Getter should not be called after ChromeBrowserState destruction.";
  return ios::WebHistoryServiceFactory::GetForBrowserState(
      weak_browser_state.get());
}

}  // namespace

TabsSearchService* TabsSearchServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TabsSearchService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

TabsSearchServiceFactory* TabsSearchServiceFactory::GetInstance() {
  static base::NoDestructor<TabsSearchServiceFactory> instance;
  return instance.get();
}

TabsSearchServiceFactory::TabsSearchServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabsSearchService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::WebHistoryServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

TabsSearchServiceFactory::~TabsSearchServiceFactory() = default;

std::unique_ptr<KeyedService> TabsSearchServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  const bool is_off_the_record = browser_state->IsOffTheRecord();
  return std::make_unique<TabsSearchService>(
      is_off_the_record, BrowserListFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      SyncServiceFactory::GetForBrowserState(browser_state),
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state),
      SessionSyncServiceFactory::GetForBrowserState(browser_state),
      is_off_the_record
          ? nullptr
          : ios::HistoryServiceFactory::GetForBrowserState(
                browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      is_off_the_record ? TabsSearchService::WebHistoryServiceGetter()
                        : base::BindRepeating(&WebHistoryServiceGetter,
                                              browser_state->AsWeakPtr()));
}

web::BrowserState* TabsSearchServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
