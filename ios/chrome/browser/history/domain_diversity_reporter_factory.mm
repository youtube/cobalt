// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/domain_diversity_reporter_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "build/build_config.h"
#import "components/history/metrics/domain_diversity_reporter.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
DomainDiversityReporter* DomainDiversityReporterFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<DomainDiversityReporter*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
DomainDiversityReporterFactory* DomainDiversityReporterFactory::GetInstance() {
  static base::NoDestructor<DomainDiversityReporterFactory> instance;
  return instance.get();
}

DomainDiversityReporterFactory::DomainDiversityReporterFactory()
    : BrowserStateKeyedServiceFactory(
          "DomainDiversityReporter",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

DomainDiversityReporterFactory::~DomainDiversityReporterFactory() = default;

std::unique_ptr<KeyedService>
DomainDiversityReporterFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  if (chrome_browser_state->IsOffTheRecord())
    return nullptr;

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  // Only build DomainDiversityReporter service with a valid `history_service`.
  if (!history_service)
    return nullptr;

  return std::make_unique<DomainDiversityReporter>(
      history_service, chrome_browser_state->GetPrefs(),
      base::DefaultClock::GetInstance());
}

void DomainDiversityReporterFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DomainDiversityReporter::RegisterProfilePrefs(registry);
}

web::BrowserState* DomainDiversityReporterFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool DomainDiversityReporterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool DomainDiversityReporterFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}
