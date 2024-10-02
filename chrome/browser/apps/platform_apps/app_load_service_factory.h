// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace apps {

class AppLoadService;

class AppLoadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AppLoadService* GetForBrowserContext(content::BrowserContext* context);

  static AppLoadServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppLoadServiceFactory>;

  AppLoadServiceFactory();
  ~AppLoadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_LOAD_SERVICE_FACTORY_H_
