// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_sensors_setting_policy_handler.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/device_features.h"

namespace policy {

struct DefaultSensorsSettingTestCase {
  bool feature_enabled;
  std::optional<ContentSetting> input_policy;
  std::optional<ContentSetting> expected_pref;
};

// Verifies `DefaultSensorsSettingPolicyHandler` translates the policy correctly
// based on the feature flag, without crashing during early browser startup
// (before `FeatureList` is initialized).
// See https://crbug.com/525409687.
class DefaultSensorsSettingPolicyHandlerTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<DefaultSensorsSettingTestCase> {
 public:
  DefaultSensorsSettingPolicyHandlerTest() {
    if (GetParam().feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSensorsAllowAskBlockPermissionModel);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSensorsAllowAskBlockPermissionModel);
    }
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    PolicyMap values;
    if (GetParam().input_policy.has_value()) {
      values.Set(key::kDefaultSensorsSetting, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                 base::Value(GetParam().input_policy.value()), nullptr);
    }
    provider_.UpdateChromePolicy(values);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_P(DefaultSensorsSettingPolicyHandlerTest,
                       TranslatePolicy) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  if (GetParam().expected_pref.has_value()) {
    EXPECT_TRUE(
        prefs->IsManagedPreference(prefs::kManagedDefaultSensorsSetting));
    EXPECT_EQ(GetParam().expected_pref.value(),
              prefs->GetInteger(prefs::kManagedDefaultSensorsSetting));
  } else {
    EXPECT_FALSE(
        prefs->IsManagedPreference(prefs::kManagedDefaultSensorsSetting));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DefaultSensorsSettingPolicyHandlerTest,
    testing::Values(
        // Flag OFF: Ask (3) falls back to Allow (1).
        DefaultSensorsSettingTestCase{/*feature_enabled=*/false,
                                      CONTENT_SETTING_ALLOW,
                                      CONTENT_SETTING_ALLOW},
        DefaultSensorsSettingTestCase{/*feature_enabled=*/false,
                                      CONTENT_SETTING_BLOCK,
                                      CONTENT_SETTING_BLOCK},
        DefaultSensorsSettingTestCase{/*feature_enabled=*/false,
                                      CONTENT_SETTING_ASK,
                                      CONTENT_SETTING_ALLOW},

        // Flag ON: Ask (3) remains Ask (3).
        DefaultSensorsSettingTestCase{/*feature_enabled=*/true,
                                      CONTENT_SETTING_ALLOW,
                                      CONTENT_SETTING_ALLOW},
        DefaultSensorsSettingTestCase{/*feature_enabled=*/true,
                                      CONTENT_SETTING_BLOCK,
                                      CONTENT_SETTING_BLOCK},
        DefaultSensorsSettingTestCase{/*feature_enabled=*/true,
                                      CONTENT_SETTING_ASK, CONTENT_SETTING_ASK},

        // Unset policy.
        DefaultSensorsSettingTestCase{/*feature_enabled=*/true, std::nullopt,
                                      std::nullopt},
        DefaultSensorsSettingTestCase{/*feature_enabled=*/false, std::nullopt,
                                      std::nullopt}));

}  // namespace policy
