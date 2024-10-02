// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web/public/browser_state.h"

// static
BreadcrumbManagerKeyedServiceFactory*
BreadcrumbManagerKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<BreadcrumbManagerKeyedServiceFactory> instance;
  return instance.get();
}

// static
breadcrumbs::BreadcrumbManagerKeyedService*
BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

BreadcrumbManagerKeyedServiceFactory::BreadcrumbManagerKeyedServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BreadcrumbManagerService",
          BrowserStateDependencyManager::GetInstance()) {}

BreadcrumbManagerKeyedServiceFactory::~BreadcrumbManagerKeyedServiceFactory() {}

std::unique_ptr<KeyedService>
BreadcrumbManagerKeyedServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<breadcrumbs::BreadcrumbManagerKeyedService>(
      browser_state->IsOffTheRecord());
}

web::BrowserState* BreadcrumbManagerKeyedServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  // Create the service for both normal and incognito browser states.
  return browser_state;
}
