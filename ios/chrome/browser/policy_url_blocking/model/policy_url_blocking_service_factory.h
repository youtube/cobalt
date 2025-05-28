// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class PolicyBlocklistService;

// Owns all PolicyBlocklistService instances and associate them to profiles.
class PolicyBlocklistServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PolicyBlocklistServiceFactory* GetInstance();
  static PolicyBlocklistService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<PolicyBlocklistServiceFactory>;

  PolicyBlocklistServiceFactory();
  ~PolicyBlocklistServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_FACTORY_H_
