// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/omnibox/browser/provider_state_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/browser_state.h"

namespace ios {

// static
ProviderStateService* ProviderStateServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ProviderStateService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ProviderStateServiceFactory* ProviderStateServiceFactory::GetInstance() {
  static base::NoDestructor<ProviderStateServiceFactory> instance;
  return instance.get();
}

ProviderStateServiceFactory::ProviderStateServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProviderStateService",
          BrowserStateDependencyManager::GetInstance()) {}

ProviderStateServiceFactory::~ProviderStateServiceFactory() = default;

std::unique_ptr<KeyedService>
ProviderStateServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ProviderStateService>();
}

}  // namespace ios
