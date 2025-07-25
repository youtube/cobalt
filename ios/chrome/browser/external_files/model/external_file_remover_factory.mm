// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_impl.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
ExternalFileRemover* ExternalFileRemoverFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ExternalFileRemover*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ExternalFileRemoverFactory* ExternalFileRemoverFactory::GetInstance() {
  static base::NoDestructor<ExternalFileRemoverFactory> instance;
  return instance.get();
}

ExternalFileRemoverFactory::ExternalFileRemoverFactory()
    : BrowserStateKeyedServiceFactory(
          "ExternalFileRemoverService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

ExternalFileRemoverFactory::~ExternalFileRemoverFactory() {}

std::unique_ptr<KeyedService>
ExternalFileRemoverFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<ExternalFileRemoverImpl>(
      browser_state,
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
}
