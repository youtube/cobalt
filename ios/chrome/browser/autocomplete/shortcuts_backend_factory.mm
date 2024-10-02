// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/shortcuts_backend_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/omnibox/browser/shortcuts_constants.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

scoped_refptr<ShortcutsBackend> CreateShortcutsBackend(
    ChromeBrowserState* browser_state,
    bool suppress_db) {
  scoped_refptr<ShortcutsBackend> shortcuts_backend(new ShortcutsBackend(
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state),
      std::make_unique<ios::UIThreadSearchTermsData>(),
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      browser_state->GetStatePath().Append(kShortcutsDatabaseName),
      suppress_db));
  return shortcuts_backend->Init() ? shortcuts_backend : nullptr;
}

}  // namespace

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return base::WrapRefCounted(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
scoped_refptr<ShortcutsBackend>
ShortcutsBackendFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return base::WrapRefCounted(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false).get()));
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  static base::NoDestructor<ShortcutsBackendFactory> instance;
  return instance.get();
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "ShortcutsBackend",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() {}

scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return CreateShortcutsBackend(browser_state, false /* suppress_db */);
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
