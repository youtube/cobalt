// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace content_settings {

class CookieSettingsPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    handler_list_.AddHandler(std::make_unique<CookieSettingsPolicyHandler>());
  }

 protected:
  void SetThirdPartyCookiePolicy(bool third_party_cookie_blocking_enabled) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kBlockThirdPartyCookies,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(third_party_cookie_blocking_enabled), nullptr);
    UpdateProviderPolicy(policy);
  }

  void SetDefaultCookiePolicy(ContentSetting content_setting) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kDefaultCookiesSetting,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(content_setting),
               nullptr);
    UpdateProviderPolicy(policy);
  }
};

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingNotSet) {
  policy::PolicyMap policy;
  UpdateProviderPolicy(policy);
  const base::Value* value;
  EXPECT_FALSE(store_->GetValue(prefs::kCookieControlsMode, &value));
}

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingEnabled) {
  SetThirdPartyCookiePolicy(true);
  const base::Value* value;
  ASSERT_TRUE(store_->GetValue(prefs::kCookieControlsMode, &value));
  EXPECT_EQ(static_cast<CookieControlsMode>(value->GetInt()),
            CookieControlsMode::kBlockThirdParty);
}

TEST_F(CookieSettingsPolicyHandlerTest, ThirdPartyCookieBlockingDisabled) {
  SetThirdPartyCookiePolicy(false);
  const base::Value* value;
  ASSERT_TRUE(store_->GetValue(prefs::kCookieControlsMode, &value));
  EXPECT_EQ(static_cast<CookieControlsMode>(value->GetInt()),
            CookieControlsMode::kOff);
}

TEST_F(CookieSettingsPolicyHandlerTest,
       ThirdPartyCookieBlockingAndCookieContentNotSetPrivacySandboxNotSet) {
  policy::PolicyMap policy;
  UpdateProviderPolicy(policy);
  const base::Value* value;
  EXPECT_FALSE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_FALSE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
}

TEST_F(CookieSettingsPolicyHandlerTest,
       ThirdPartyCookieBlockingSetTruePrivacySandboxDisabled) {
  SetThirdPartyCookiePolicy(true);
  const base::Value* value;
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_EQ(value->GetBool(), false);
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
  EXPECT_EQ(value->GetBool(), false);
}

TEST_F(CookieSettingsPolicyHandlerTest,
       ThirdPartyCookieBlockingSetFalsePrivacySandboxEnabled) {
  SetThirdPartyCookiePolicy(false);
  const base::Value* value;
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_EQ(value->GetBool(), true);
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
  EXPECT_EQ(value->GetBool(), true);
}

TEST_F(CookieSettingsPolicyHandlerTest,
       DefaultCookieContentBlockAllPrivacySandboxDisabled) {
  SetDefaultCookiePolicy(CONTENT_SETTING_BLOCK);
  const base::Value* value;
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_EQ(value->GetBool(), false);
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
  EXPECT_EQ(value->GetBool(), false);
}

TEST_F(CookieSettingsPolicyHandlerTest,
       DefaultCookieContentAllowedPrivacySandboxNotSet) {
  SetDefaultCookiePolicy(CONTENT_SETTING_ALLOW);
  const base::Value* value;
  EXPECT_FALSE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_FALSE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
}

TEST_F(CookieSettingsPolicyHandlerTest,
       DefaultCookieContentBlockOverridesThirdPartyForPrivacySandbox) {
  // A policy which sets the default cookie content setting to block should
  // override a policy enabling 3P cookies, w.r.t the Privacy Sandbox.
  policy::PolicyMap policy;
  policy.Set(policy::key::kBlockThirdPartyCookies,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy.Set(policy::key::kDefaultCookiesSetting,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(CONTENT_SETTING_BLOCK),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value;
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabled, &value));
  EXPECT_EQ(value->GetBool(), false);
  EXPECT_TRUE(store_->GetValue(prefs::kPrivacySandboxApisEnabledV2, &value));
  EXPECT_EQ(value->GetBool(), false);
}

}  // namespace content_settings
