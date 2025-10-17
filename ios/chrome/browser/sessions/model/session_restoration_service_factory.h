// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_

#import <memory>

#import "base/functional/callback_forward.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class SessionRestorationService;

// Singleton that owns all SessionRestorationService and associates them with
// ProfileIOS.
class SessionRestorationServiceFactory final
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Represents the storage format that is requested.
  enum StorageFormat {
    kLegacy,
    kOptimized,
  };

  static SessionRestorationService* GetForProfile(ProfileIOS* profile);
  static SessionRestorationServiceFactory* GetInstance();

  // Requests that session storage for `profile` is migrated if needed.
  // Invokes `closure` when the migration is complete. If data is already in
  // the correct format, `closure` is called synchronously.
  //
  // Must be called before GetForProfile() is called for `profile`.
  void MigrateSessionStorageFormat(ProfileIOS* profile,
                                   StorageFormat requested_format,
                                   base::OnceClosure closure);

 private:
  friend class base::NoDestructor<SessionRestorationServiceFactory>;

  SessionRestorationServiceFactory();
  ~SessionRestorationServiceFactory() final;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
