// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class PhotosService;

// Singleton that owns all PhotosService-s and associates them with
// ChromeBrowserState.
class PhotosServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PhotosService* GetForBrowserState(ChromeBrowserState* browser_state);
  static PhotosServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PhotosServiceFactory>;

  PhotosServiceFactory();
  ~PhotosServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_FACTORY_H_
