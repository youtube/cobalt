// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service_factory.h"

#import "base/feature_list.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_configuration.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/drive/drive_api.h"

namespace drive {

// static
DriveService* DriveServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DriveService>(profile,
                                                             /*create=*/true);
}

// static
DriveServiceFactory* DriveServiceFactory::GetInstance() {
  static base::NoDestructor<DriveServiceFactory> instance;
  return instance.get();
}

DriveServiceFactory::DriveServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DriveService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
}

DriveServiceFactory::~DriveServiceFactory() = default;

std::unique_ptr<KeyedService> DriveServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<DriveService> overridden_drive_service =
      tests_hook::GetOverriddenDriveService();
  if (overridden_drive_service) {
    return overridden_drive_service;
  }

  drive::DriveServiceConfiguration configuration{};
  configuration.sso_service = GetApplicationContext()->GetSingleSignOnService();
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  configuration.pref_service = profile->GetPrefs();
  configuration.identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  configuration.account_manager_service =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  return ios::provider::CreateDriveService(configuration);
}

}  // namespace drive
