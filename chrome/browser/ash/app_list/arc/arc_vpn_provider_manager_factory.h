// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace app_list {

class ArcVpnProviderManager;

class ArcVpnProviderManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ArcVpnProviderManagerFactory(const ArcVpnProviderManagerFactory&) = delete;
  ArcVpnProviderManagerFactory& operator=(const ArcVpnProviderManagerFactory&) =
      delete;

  static ArcVpnProviderManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcVpnProviderManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ArcVpnProviderManagerFactory>;

  ArcVpnProviderManagerFactory();
  ~ArcVpnProviderManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_FACTORY_H_
