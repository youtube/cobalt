// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

std::unique_ptr<KeyedService> BuildServiceInstance(web::BrowserState* context) {
  CHECK(IsSafetyCheckMagicStackEnabled());

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  const scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  return std::make_unique<IOSChromeSafetyCheckManager>(
      profile->GetPrefs(), GetApplicationContext()->GetLocalState(),
      IOSChromePasswordCheckManagerFactory::GetForProfile(profile),
      task_runner);
}

}  // namespace

// static
IOSChromeSafetyCheckManager* IOSChromeSafetyCheckManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<IOSChromeSafetyCheckManager>(
      profile, /*create=*/true);
}

// static
IOSChromeSafetyCheckManagerFactory*
IOSChromeSafetyCheckManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromeSafetyCheckManagerFactory> instance;
  return instance.get();
}

// static
IOSChromeSafetyCheckManagerFactory::TestingFactory
IOSChromeSafetyCheckManagerFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildServiceInstance);
}

IOSChromeSafetyCheckManagerFactory::IOSChromeSafetyCheckManagerFactory()
    : ProfileKeyedServiceFactoryIOS("SafetyCheckManager",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(IOSChromePasswordCheckManagerFactory::GetInstance());
}

IOSChromeSafetyCheckManagerFactory::~IOSChromeSafetyCheckManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeSafetyCheckManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildServiceInstance(context);
}
