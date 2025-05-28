// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DOM_DISTILLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DOM_DISTILLER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace dom_distiller {

class DomDistillerService;

class DomDistillerServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DomDistillerService* GetForProfile(ProfileIOS* profile);
  static DomDistillerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DomDistillerServiceFactory>;

  DomDistillerServiceFactory();
  ~DomDistillerServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DOM_DISTILLER_SERVICE_FACTORY_H_
