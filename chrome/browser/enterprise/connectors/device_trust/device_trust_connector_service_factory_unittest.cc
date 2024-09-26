// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;

namespace enterprise_connectors {

class DeviceTrustConnectorServiceFactoryBaseTest {
 public:
  DeviceTrustConnectorServiceFactoryBaseTest() {
    RedirectTestingFactoryToRealFactory(profile());
  }

  // TestingProfiles create use a TestingFactory for service
  // `DeviceTrustConnectorService`. To test the real factory behavior via a unit
  // test, we redirect the testing factory to the real factory
  // `BuildServiceInstanceFor` method.
  void RedirectTestingFactoryToRealFactory(Profile* profile) {
    DeviceTrustConnectorServiceFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return DeviceTrustConnectorServiceFactory::GetInstance()
              ->BuildServiceInstanceForBrowserContext(context);
        }));
  }

 protected:
  Profile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  base::test::ScopedFeatureList feature_list_;
};

class DeviceTrustConnectorServiceFactoryTest
    : public DeviceTrustConnectorServiceFactoryBaseTest,
      public ::testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled, true);
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DeviceTrustConnectorServiceFactoryTest, CreateForRegularProfile) {
  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_TRUE(DeviceTrustConnectorServiceFactory::GetForProfile(profile()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(DeviceTrustConnectorServiceFactoryTest, NullForIncognitoProfile) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(ash::ProfileHelper::IsSigninProfile(incognito_profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ASSERT_TRUE(incognito_profile);

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(device_trust_connector_service);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DeviceTrustConnectorServiceFactoryParamTest
    : public DeviceTrustConnectorServiceFactoryBaseTest,
      public ::testing::TestWithParam<bool> {
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features{
        enterprise_connectors::kDeviceTrustConnectorEnabled};
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam()) {
      enabled_features.push_back(
          ash::features::kLoginScreenDeviceTrustConnectorEnabled);
    } else {
      disabled_features.push_back(
          ash::features::kLoginScreenDeviceTrustConnectorEnabled);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
};

TEST_P(DeviceTrustConnectorServiceFactoryParamTest,
       CreatedForSigninProfileChromeOS) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
  std::unique_ptr<TestingProfile> testing_profile = builder.Build();

  Profile* signin_profile =
      testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  RedirectTestingFactoryToRealFactory(signin_profile);

  ASSERT_TRUE(signin_profile);
  EXPECT_TRUE(signin_profile->IsOffTheRecord());
  EXPECT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile));

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(signin_profile);
  EXPECT_EQ(device_trust_connector_service != nullptr, GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceTrustConnectorServiceFactoryParamTest,
                         ::testing::Values(true, false));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors
