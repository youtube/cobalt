// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace ash {

class LoginScreenExtensionsLifetimeManager;

class LoginScreenExtensionsLifetimeManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static LoginScreenExtensionsLifetimeManager* GetForProfile(Profile* profile);
  static LoginScreenExtensionsLifetimeManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<LoginScreenExtensionsLifetimeManagerFactory>;

  LoginScreenExtensionsLifetimeManagerFactory();
  ~LoginScreenExtensionsLifetimeManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXTENSIONS_LOGIN_SCREEN_EXTENSIONS_LIFETIME_MANAGER_FACTORY_H_
