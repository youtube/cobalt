// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace password_manager {
class PasswordReuseManager;
}

// Creates instances of PasswordReuseManager per profile.
class IOSChromePasswordReuseManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static password_manager::PasswordReuseManager* GetForProfile(
      ProfileIOS* profile);
  static IOSChromePasswordReuseManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromePasswordReuseManagerFactory>;

  IOSChromePasswordReuseManagerFactory();
  ~IOSChromePasswordReuseManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
