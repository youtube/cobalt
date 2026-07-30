// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace policy {

class EnterpriseManagementMetricsService;

// Factory for creating EnterpriseManagementMetricsService instances per profile
// on iOS.
class EnterpriseManagementMetricsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static EnterpriseManagementMetricsServiceFactory* GetInstance();
  static EnterpriseManagementMetricsService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<EnterpriseManagementMetricsServiceFactory>;

  EnterpriseManagementMetricsServiceFactory();
  ~EnterpriseManagementMetricsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS overrides:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_FACTORY_H_
