// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"

#import "base/no_destructor.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace feature_engagement {

// static
TrackerFactory* TrackerFactory::GetInstance() {
  static base::NoDestructor<TrackerFactory> instance;
  return instance.get();
}

// static
feature_engagement::Tracker* TrackerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<feature_engagement::Tracker*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

TrackerFactory::TrackerFactory()
    : BrowserStateKeyedServiceFactory(
          "feature_engagement::Tracker",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(PromosManagerEventExporterFactory::GetInstance());
}

TrackerFactory::~TrackerFactory() = default;

std::unique_ptr<KeyedService> TrackerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return CreateFeatureEngagementTracker(context);
}

// Finds which browser state to use. If `context` is an incognito browser
// state, it returns the non-incognito state. Thus, feature engagement events
// are tracked even in incognito tabs.
web::BrowserState* TrackerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace feature_engagement
