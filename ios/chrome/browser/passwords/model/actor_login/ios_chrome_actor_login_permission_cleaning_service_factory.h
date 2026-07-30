// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace actor_login {
class ActorLoginPermissionCleaningService;
}  // namespace actor_login

// Singleton that owns all `ActorLoginPermissionCleaningService` instances and
// associates them with `ProfileIOS` instances.
class IOSChromeActorLoginPermissionCleaningServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static actor_login::ActorLoginPermissionCleaningService* GetForProfile(
      ProfileIOS* profile);

  static IOSChromeActorLoginPermissionCleaningServiceFactory* GetInstance();

  IOSChromeActorLoginPermissionCleaningServiceFactory(
      const IOSChromeActorLoginPermissionCleaningServiceFactory&) = delete;
  IOSChromeActorLoginPermissionCleaningServiceFactory& operator=(
      const IOSChromeActorLoginPermissionCleaningServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<
      IOSChromeActorLoginPermissionCleaningServiceFactory>;

  IOSChromeActorLoginPermissionCleaningServiceFactory();
  ~IOSChromeActorLoginPermissionCleaningServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_CLEANING_SERVICE_FACTORY_H_
