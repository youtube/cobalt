// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service_interface.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeBulkLeakCheckServiceFactory*
IOSChromeBulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckServiceInterface*
IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<password_manager::BulkLeakCheckServiceInterface*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeBulkLeakCheckServiceFactory::IOSChromeBulkLeakCheckServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordBulkLeakCheck",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSChromeBulkLeakCheckServiceFactory::~IOSChromeBulkLeakCheckServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeBulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<password_manager::BulkLeakCheckService>(
      IdentityManagerFactory::GetForBrowserState(browser_state),
      context->GetSharedURLLoaderFactory());
}
