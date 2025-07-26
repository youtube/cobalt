// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

enum class ServiceAccessType;

namespace history {
class HistoryService;
}

namespace ios {
// Singleton that owns all HistoryServices and associates them with
// ProfileIOS.
class HistoryServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static history::HistoryService* GetForProfile(ProfileIOS* profile,
                                                ServiceAccessType access_type);
  static HistoryServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<HistoryServiceFactory>;

  HistoryServiceFactory();
  ~HistoryServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_SERVICE_FACTORY_H_
