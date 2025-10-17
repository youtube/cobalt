// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace {

// Returns the country.
std::optional<std::string> GetCountry() {
  return safe_browsing::hash_realtime_utils::GetCountryCode(
      GetApplicationContext()->GetVariationsService());
}

// Default factory.
std::unique_ptr<KeyedService> BuildOhttpKeyService(web::BrowserState* context) {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          safe_browsing_service->GetURLLoaderFactory());
  return std::make_unique<safe_browsing::OhttpKeyService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      profile->GetPrefs(), GetApplicationContext()->GetLocalState(),
      base::BindRepeating(&GetCountry),
      /*are_background_lookups_allowed=*/false);
}

}  // namespace

// static
safe_browsing::OhttpKeyService* OhttpKeyServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<safe_browsing::OhttpKeyService>(
      profile, /*create=*/true);
}

// static
OhttpKeyServiceFactory* OhttpKeyServiceFactory::GetInstance() {
  static base::NoDestructor<OhttpKeyServiceFactory> instance;
  return instance.get();
}

// static
OhttpKeyServiceFactory::TestingFactory
OhttpKeyServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildOhttpKeyService);
}

// The service is created early to start async key fetch.
OhttpKeyServiceFactory::OhttpKeyServiceFactory()
    : ProfileKeyedServiceFactoryIOS("OhttpKeyService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService> OhttpKeyServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildOhttpKeyService(context);
}
