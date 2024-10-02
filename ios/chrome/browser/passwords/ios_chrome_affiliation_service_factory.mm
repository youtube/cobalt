// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/password_manager/core/browser/affiliation/affiliation_service_impl.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
IOSChromeAffiliationServiceFactory*
IOSChromeAffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAffiliationServiceFactory> instance;
  return instance.get();
}

// static
password_manager::AffiliationService*
IOSChromeAffiliationServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::AffiliationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeAffiliationServiceFactory::IOSChromeAffiliationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeAffiliationServiceFactory::~IOSChromeAffiliationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeAffiliationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  auto affiliation_service =
      std::make_unique<password_manager::AffiliationServiceImpl>(
          context->GetSharedURLLoaderFactory(), backend_task_runner,
          ChromeBrowserState::FromBrowserState(context)->GetPrefs());

  base::FilePath database_path = context->GetStatePath().Append(
      password_manager::kAffiliationDatabaseFileName);
  affiliation_service->Init(
      GetApplicationContext()->GetNetworkConnectionTracker(), database_path);

  return affiliation_service;
}
