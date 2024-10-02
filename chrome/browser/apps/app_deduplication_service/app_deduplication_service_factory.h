// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps::deduplication {

class AppDeduplicationService;

// Singleton that owns all AppDeduplicationService instances and associates
// them with Profile.
class AppDeduplicationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AppDeduplicationService* GetForProfile(Profile* profile);
  static AppDeduplicationServiceFactory* GetInstance();

  static bool IsAppDeduplicationServiceAvailableForProfile(Profile* profile);

  AppDeduplicationServiceFactory(const AppDeduplicationServiceFactory&) =
      delete;
  AppDeduplicationServiceFactory& operator=(
      const AppDeduplicationServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AppDeduplicationServiceFactory>;

  AppDeduplicationServiceFactory();
  ~AppDeduplicationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace apps::deduplication

#endif  // CHROME_BROWSER_APPS_APP_DEDUPLICATION_SERVICE_APP_DEDUPLICATION_SERVICE_FACTORY_H_
