// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class InitialWebUIProfileService;

class InitialWebUIProfileServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static InitialWebUIProfileService* GetForProfile(Profile* profile);
  static InitialWebUIProfileServiceFactory* GetInstance();

  InitialWebUIProfileServiceFactory(const InitialWebUIProfileServiceFactory&) =
      delete;
  InitialWebUIProfileServiceFactory& operator=(
      const InitialWebUIProfileServiceFactory&) = delete;

 private:
  friend base::NoDestructor<InitialWebUIProfileServiceFactory>;

  InitialWebUIProfileServiceFactory();
  ~InitialWebUIProfileServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_PROFILE_SERVICE_FACTORY_H_
