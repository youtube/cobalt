// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_sensors_setting_policy_handler.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

struct DefaultSensorsSettingTestCase {
  bool feature_enabled;
  ContentSetting input_policy;
  ContentSetting expected_pref;
};

class DefaultSensorsSettingPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<DefaultSensorsSettingTestCase> {
 protected:
  void SetUp() override {
    handler_list_.AddHandler(
        std::make_unique<DefaultSensorsSettingPolicyHandler>());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(DefaultSensorsSettingPolicyHandlerTest, TranslatePolicy) {
  const DefaultSensorsSettingTestCase& test_case = GetParam();
  if (test_case.feature_enabled) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSensorsAllowAskBlockPermissionModel);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        features::kSensorsAllowAskBlockPermissionModel);
  }

  PolicyMap policy;
  policy.Set(key::kDefaultSensorsSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(test_case.input_policy), nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultSensorsSetting, &value));
  EXPECT_EQ(base::Value(test_case.expected_pref), *value);
}

TEST_F(DefaultSensorsSettingPolicyHandlerTest, PolicyUnset) {
  PolicyMap policy;
  UpdateProviderPolicy(policy);

  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultSensorsSetting, nullptr));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DefaultSensorsSettingPolicyHandlerTest,
                         testing::Values(
                             // Flag OFF: Ask (3) falls back to Allow (1).
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/false,
                                 CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW},
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/false,
                                 CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK},
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/false, CONTENT_SETTING_ASK,
                                 CONTENT_SETTING_ALLOW},

                             // Flag ON: Ask (3) remains Ask (3).
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/true,
                                 CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW},
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/true,
                                 CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK},
                             DefaultSensorsSettingTestCase{
                                 /*feature_enabled=*/true, CONTENT_SETTING_ASK,
                                 CONTENT_SETTING_ASK}));

}  // namespace policy
