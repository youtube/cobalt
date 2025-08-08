// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class HistoryClustersModuleService;
class Profile;

class HistoryClustersModuleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static HistoryClustersModuleService* GetForProfile(Profile* profile);
  static HistoryClustersModuleServiceFactory* GetInstance();
  HistoryClustersModuleServiceFactory(
      const HistoryClustersModuleServiceFactory&) = delete;

 private:
  friend base::NoDestructor<HistoryClustersModuleServiceFactory>;
  HistoryClustersModuleServiceFactory();
  ~HistoryClustersModuleServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_FACTORY_H_
