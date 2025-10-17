// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace password_manager {
class CredentialsCleanerRunner;
}  // namespace password_manager

// Creates instances of CredentialsCleanerRunner per Profile.
class CredentialsCleanerRunnerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CredentialsCleanerRunnerFactory* GetInstance();
  static password_manager::CredentialsCleanerRunner* GetForProfile(
      ProfileIOS* profile);

 private:
  friend class base::NoDestructor<CredentialsCleanerRunnerFactory>;

  CredentialsCleanerRunnerFactory();
  ~CredentialsCleanerRunnerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
