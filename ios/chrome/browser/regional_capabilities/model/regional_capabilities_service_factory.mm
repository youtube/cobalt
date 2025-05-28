// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/country_codes/country_codes.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

namespace {

class RegionalCapabilitiesServiceClient
    : public regional_capabilities::RegionalCapabilitiesService::Client {
 public:
  RegionalCapabilitiesServiceClient() = default;

  int GetFallbackCountryId() override {
    return country_codes::GetCurrentCountryID();
  }

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override {
    std::move(country_id_fetched_callback)
        .Run(country_codes::GetCurrentCountryID());
  }
};

}  // namespace

RegionalCapabilitiesServiceFactory::RegionalCapabilitiesServiceFactory()
    : ProfileKeyedServiceFactoryIOS("RegionalCapabilitiesService",
                                    ProfileSelection::kRedirectedInIncognito) {}

RegionalCapabilitiesServiceFactory::~RegionalCapabilitiesServiceFactory() =
    default;

// static
RegionalCapabilitiesServiceFactory*
RegionalCapabilitiesServiceFactory::GetInstance() {
  static base::NoDestructor<RegionalCapabilitiesServiceFactory> factory;
  return factory.get();
}

// static
regional_capabilities::RegionalCapabilitiesService*
RegionalCapabilitiesServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          regional_capabilities::RegionalCapabilitiesService>(profile,
                                                              /*create=*/true);
}

std::unique_ptr<KeyedService>
RegionalCapabilitiesServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  return std::make_unique<regional_capabilities::RegionalCapabilitiesService>(
      CHECK_DEREF(profile->GetPrefs()),
      std::make_unique<RegionalCapabilitiesServiceClient>());
}

}  // namespace ios
