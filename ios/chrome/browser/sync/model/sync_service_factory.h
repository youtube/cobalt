// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace syncer {
class SyncServiceImpl;
class SyncService;
}  // namespace syncer

// Singleton that owns all SyncServices and associates them with
// ProfileIOS.
class SyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::SyncService* GetForProfile(ProfileIOS* profile);
  static syncer::SyncService* GetForProfileIfExists(ProfileIOS* profile);

  static syncer::SyncServiceImpl* GetAsSyncServiceImplForBrowserStateForTesting(
      ProfileIOS* profile);

  static SyncServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SyncServiceFactory>;

  SyncServiceFactory();
  ~SyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SERVICE_FACTORY_H_
