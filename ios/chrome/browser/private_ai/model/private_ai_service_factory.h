// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace private_ai {
class PrivateAiService;
}  // namespace private_ai

class PrivateAiServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static private_ai::PrivateAiService* GetForProfile(ProfileIOS* profile);
  static PrivateAiServiceFactory* GetInstance();

  PrivateAiServiceFactory(const PrivateAiServiceFactory&) = delete;
  PrivateAiServiceFactory& operator=(const PrivateAiServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PrivateAiServiceFactory>;

  PrivateAiServiceFactory();
  ~PrivateAiServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PRIVATE_AI_MODEL_PRIVATE_AI_SERVICE_FACTORY_H_
