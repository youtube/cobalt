// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class TrustedVaultClientBackend;

// Singleton that owns all TrustedVaultClientBackends and associates them with
// ProfileIOS.
class TrustedVaultClientBackendFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TrustedVaultClientBackend* GetForProfile(ProfileIOS* profile);
  static TrustedVaultClientBackendFactory* GetInstance();

 private:
  friend class base::NoDestructor<TrustedVaultClientBackendFactory>;

  TrustedVaultClientBackendFactory();
  ~TrustedVaultClientBackendFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_FACTORY_H_
