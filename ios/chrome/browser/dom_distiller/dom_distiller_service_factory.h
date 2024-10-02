// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace dom_distiller {
class DomDistillerService;
}

namespace dom_distiller {

class DomDistillerServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static DomDistillerServiceFactory* GetInstance();
  static DomDistillerService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  DomDistillerServiceFactory(const DomDistillerServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<DomDistillerServiceFactory>;

  DomDistillerServiceFactory();
  ~DomDistillerServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_DOM_DISTILLER_SERVICE_FACTORY_H_
