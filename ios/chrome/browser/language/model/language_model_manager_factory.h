// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace language {
class LanguageModelManager;
}

// Manages the language model for each profile. The particular language model
// provided depends on feature flags.
class LanguageModelManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static language::LanguageModelManager* GetForProfile(ProfileIOS* profile);
  static LanguageModelManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<LanguageModelManagerFactory>;

  LanguageModelManagerFactory();
  ~LanguageModelManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_
