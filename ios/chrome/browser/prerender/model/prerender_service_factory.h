// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class PrerenderService;
class ProfileIOS;

// Singleton that creates the PrerenderService and associates that service with
// profile.
class PrerenderServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PrerenderService* GetForProfile(ProfileIOS* profile);
  static PrerenderServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PrerenderServiceFactory>;

  PrerenderServiceFactory();
  ~PrerenderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_
