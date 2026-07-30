// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_permission_cleaning_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/actor_login/internal/actor_login_permission_cleaning_service.h"
#import "components/password_manager/core/browser/actor_login/internal/actor_login_permission_cleaning_service_impl.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_permission_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using actor_login::ActorLoginPermissionCleaningService;
using actor_login::ActorLoginPermissionCleaningServiceImpl;

// static
ActorLoginPermissionCleaningService*
IOSChromeActorLoginPermissionCleaningServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<ActorLoginPermissionCleaningService>(
          profile, /*create=*/true);
}

// static
IOSChromeActorLoginPermissionCleaningServiceFactory*
IOSChromeActorLoginPermissionCleaningServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeActorLoginPermissionCleaningServiceFactory>
      instance;
  return instance.get();
}

IOSChromeActorLoginPermissionCleaningServiceFactory::
    IOSChromeActorLoginPermissionCleaningServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ActorLoginDuplicatePermissionCleaner") {
  DependsOn(IOSChromeActorLoginPermissionServiceFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
}

IOSChromeActorLoginPermissionCleaningServiceFactory::
    ~IOSChromeActorLoginPermissionCleaningServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeActorLoginPermissionCleaningServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ActorLoginPermissionCleaningServiceImpl>(
      IOSChromeActorLoginPermissionServiceFactory::GetForProfile(profile),
      IOSChromeProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS));
}
