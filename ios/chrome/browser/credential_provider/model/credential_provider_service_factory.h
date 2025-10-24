// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class CredentialProviderService;
class ProfileIOS;

// Singleton that owns all CredentialProviderServices and associates them with
// profiles.
class CredentialProviderServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CredentialProviderService* GetForProfile(ProfileIOS* profile);
  static CredentialProviderServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CredentialProviderServiceFactory>;

  CredentialProviderServiceFactory();
  ~CredentialProviderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
