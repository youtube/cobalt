// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const int kDipForServerRequests = 32;
const favicon_base::IconType kIconTypeForServerRequests =
    favicon_base::IconType::kTouchIcon;
const char kGoogleServerClientParam[] = "chrome";

std::unique_ptr<KeyedService> BuildLargeIconService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<favicon::LargeIconServiceImpl>(
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          browser_state->GetSharedURLLoaderFactory()),
      kDipForServerRequests, kIconTypeForServerRequests,
      kGoogleServerClientParam);
}

}  // namespace

// static
favicon::LargeIconService* IOSChromeLargeIconServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<favicon::LargeIconService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeLargeIconServiceFactory*
IOSChromeLargeIconServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeLargeIconServiceFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSChromeLargeIconServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildLargeIconService);
}

IOSChromeLargeIconServiceFactory::IOSChromeLargeIconServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "LargeIconService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::FaviconServiceFactory::GetInstance());
}

IOSChromeLargeIconServiceFactory::~IOSChromeLargeIconServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeLargeIconServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildLargeIconService(context);
}

web::BrowserState* IOSChromeLargeIconServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool IOSChromeLargeIconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
