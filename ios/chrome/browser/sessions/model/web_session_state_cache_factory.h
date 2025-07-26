// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
@class WebSessionStateCache;

// Singleton that owns all WebSessionStateCaches and associates them with
// ProfileIOS.
class WebSessionStateCacheFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static WebSessionStateCache* GetForProfile(ProfileIOS* profile);
  static WebSessionStateCacheFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebSessionStateCacheFactory>;

  WebSessionStateCacheFactory();
  ~WebSessionStateCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_CACHE_FACTORY_H_
