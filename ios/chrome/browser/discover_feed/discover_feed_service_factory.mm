// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_configuration.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
DiscoverFeedService* DiscoverFeedServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<DiscoverFeedService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
DiscoverFeedServiceFactory* DiscoverFeedServiceFactory::GetInstance() {
  static base::NoDestructor<DiscoverFeedServiceFactory> instance;
  return instance.get();
}

DiscoverFeedServiceFactory::DiscoverFeedServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DiscoverFeedService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DiscoverFeedServiceFactory::~DiscoverFeedServiceFactory() = default;

std::unique_ptr<KeyedService>
DiscoverFeedServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  DiscoverFeedConfiguration* configuration =
      [[DiscoverFeedConfiguration alloc] init];
  configuration.prefService = browser_state->GetPrefs();
  configuration.authService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  configuration.identityManager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  configuration.metricsRecorder = [[FeedMetricsRecorder alloc] init];
  configuration.ssoService = GetApplicationContext()->GetSSOService();

  return ios::provider::CreateDiscoverFeedService(configuration);
}
