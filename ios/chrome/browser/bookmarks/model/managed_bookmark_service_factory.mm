// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_util.h"

namespace {

std::string GetManagedBookmarksDomain(ProfileIOS* profile) {
  id<SystemIdentity> identity = GetPrimarySystemIdentity(
      signin::ConsentLevel::kSignin,
      IdentityManagerFactory::GetForProfile(profile),
      ChromeAccountManagerServiceFactory::GetForProfile(profile));
  if (!identity) {
    return std::string();
  }

  return base::SysNSStringToUTF8(
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->GetCachedHostedDomainForIdentity(identity));
}

std::unique_ptr<KeyedService> BuildManagedBookmarkModel(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  // base::Unretained is safe because ManagedBookmarkService will
  // be destroyed before the profile it is attached to.
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      profile->GetPrefs(), base::BindRepeating(&GetManagedBookmarksDomain,
                                               base::Unretained(profile)));
}

}  // namespace

// static
bookmarks::ManagedBookmarkService* ManagedBookmarkServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<bookmarks::ManagedBookmarkService>(
          profile, /*create=*/true);
}

// static
ManagedBookmarkServiceFactory* ManagedBookmarkServiceFactory::GetInstance() {
  static base::NoDestructor<ManagedBookmarkServiceFactory> instance;
  return instance.get();
}

// static
ManagedBookmarkServiceFactory::TestingFactory
ManagedBookmarkServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildManagedBookmarkModel);
}

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ManagedBookmarkService",
                                    TestingCreation::kNoServiceForTests) {}

ManagedBookmarkServiceFactory::~ManagedBookmarkServiceFactory() {}

std::unique_ptr<KeyedService>
ManagedBookmarkServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildManagedBookmarkModel(context);
}
