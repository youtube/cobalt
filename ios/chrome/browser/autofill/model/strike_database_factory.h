// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {

class StrikeDatabase;

// Singleton that owns all StrikeDatabases and associates them with
// ProfileIOS.
class StrikeDatabaseFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static StrikeDatabase* GetForProfile(ProfileIOS* profile);
  static StrikeDatabaseFactory* GetInstance();

 private:
  friend class base::NoDestructor<StrikeDatabaseFactory>;

  StrikeDatabaseFactory();
  ~StrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_
