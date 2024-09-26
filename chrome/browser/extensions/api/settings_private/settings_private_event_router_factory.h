// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class SettingsPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the settingsPrivate event router per profile (since the
// extension event router is per profile).
class SettingsPrivateEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  SettingsPrivateEventRouterFactory(const SettingsPrivateEventRouterFactory&) =
      delete;
  SettingsPrivateEventRouterFactory& operator=(
      const SettingsPrivateEventRouterFactory&) = delete;

  // Returns the SettingsPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static SettingsPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the SettingsPrivateEventRouterFactory instance.
  static SettingsPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<SettingsPrivateEventRouterFactory>;

  SettingsPrivateEventRouterFactory();
  ~SettingsPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_FACTORY_H_
