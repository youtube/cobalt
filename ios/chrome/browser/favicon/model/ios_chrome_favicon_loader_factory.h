// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_FAVICON_LOADER_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_FAVICON_LOADER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class FaviconLoader;
class ProfileIOS;

// Singleton that owns all FaviconLoaders and associates them with
// ProfileIOS.
class IOSChromeFaviconLoaderFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static FaviconLoader* GetForProfile(ProfileIOS* profile);
  static FaviconLoader* GetForProfileIfExists(ProfileIOS* profile);

  static IOSChromeFaviconLoaderFactory* GetInstance();
  // Returns the default factory used to build FaviconLoader. Can be registered
  // with SetTestingFactory to use the FaviconService instance during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<IOSChromeFaviconLoaderFactory>;

  IOSChromeFaviconLoaderFactory();
  ~IOSChromeFaviconLoaderFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_FAVICON_LOADER_FACTORY_H_
