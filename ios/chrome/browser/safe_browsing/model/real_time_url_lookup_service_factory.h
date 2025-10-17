// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace safe_browsing {
class RealTimeUrlLookupService;
}

// Singleton that owns RealTimeUrlLookupService objects, one for each active
// profile. It returns nullptr for Incognito profiles.
class RealTimeUrlLookupServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns null if `profile` is in Incognito mode.
  static safe_browsing::RealTimeUrlLookupService* GetForProfile(
      ProfileIOS* profile);
  static RealTimeUrlLookupServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RealTimeUrlLookupServiceFactory>;

  RealTimeUrlLookupServiceFactory();
  ~RealTimeUrlLookupServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
