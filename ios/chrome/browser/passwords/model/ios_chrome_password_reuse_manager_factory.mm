// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_manager_factory.h"

#import "base/no_destructor.h"
#import "build/build_config.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_reuse_detector_impl.h"
#import "components/password_manager/core/browser/password_reuse_manager_impl.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
IOSChromePasswordReuseManagerFactory*
IOSChromePasswordReuseManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordReuseManagerFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordReuseManager*
IOSChromePasswordReuseManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<password_manager::PasswordReuseManager>(
          profile, /*create=*/true);
}

IOSChromePasswordReuseManagerFactory::IOSChromePasswordReuseManagerFactory()
    : ProfileKeyedServiceFactoryIOS("PasswordReuseManager",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
}

IOSChromePasswordReuseManagerFactory::~IOSChromePasswordReuseManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromePasswordReuseManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled));
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<password_manager::PasswordReuseManager> reuse_manager =
      std::make_unique<password_manager::PasswordReuseManagerImpl>();

  reuse_manager->Init(
      profile->GetPrefs(), GetApplicationContext()->GetLocalState(),
      IOSChromeProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      std::make_unique<password_manager::PasswordReuseDetectorImpl>());
  return reuse_manager;
}
