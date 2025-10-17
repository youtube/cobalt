// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace safe_browsing {
class TailoredSecurityService;
}

// Singleton that owns TailoredSecurityService objects, one for each active
// profile. It returns nullptr for Incognito profiles.
class TailoredSecurityServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static safe_browsing::TailoredSecurityService* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of TailoredSecurityServiceFactory.
  static TailoredSecurityServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TailoredSecurityServiceFactory>;

  TailoredSecurityServiceFactory();
  ~TailoredSecurityServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
