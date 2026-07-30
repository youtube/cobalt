// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace policy {

class EnterpriseManagementMetricsService;

// Factory for creating EnterpriseManagementMetricsService instances per
// profile.
class EnterpriseManagementMetricsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  EnterpriseManagementMetricsServiceFactory(
      const EnterpriseManagementMetricsServiceFactory&) = delete;
  EnterpriseManagementMetricsServiceFactory& operator=(
      const EnterpriseManagementMetricsServiceFactory&) = delete;

  static EnterpriseManagementMetricsServiceFactory* GetInstance();
  static EnterpriseManagementMetricsService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<EnterpriseManagementMetricsServiceFactory>;

  EnterpriseManagementMetricsServiceFactory();
  ~EnterpriseManagementMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_
