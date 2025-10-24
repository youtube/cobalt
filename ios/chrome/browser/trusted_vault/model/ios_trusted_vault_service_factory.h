// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace trusted_vault {
class TrustedVaultService;
}  // namespace trusted_vault

// Owns all TrustedVaultService instances and associates them to profiles.
class IOSTrustedVaultServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static trusted_vault::TrustedVaultService* GetForProfile(ProfileIOS* profile);
  static IOSTrustedVaultServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSTrustedVaultServiceFactory>;

  IOSTrustedVaultServiceFactory();
  ~IOSTrustedVaultServiceFactory() override;

  // BrowserStateKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_
