// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/isolate_origins_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class IsolateOriginsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(const std::string& policy_name, base::Value value) {
    policies_.Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  void ApplyPolicies() { handler_.ApplyPolicySettings(policies_, &prefs_); }

  IsolateOriginsPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

#if BUILDFLAG(IS_ANDROID)

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_AndroidLegacy) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "https://example.com");
}

TEST_F(IsolateOriginsPolicyHandlerTest,
       ApplyPolicySettings_AndroidIgnoreDesktop) {
  SetPolicy("IsolateOrigins", base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_ValidAndroid) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value("https://example.com"));
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_InvalidAndroid) {
  SetPolicy(key::kIsolateOriginsAndroid, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

#else  // BUILDFLAG(IS_ANDROID)

TEST_F(IsolateOriginsPolicyHandlerTest, ApplyPolicySettings_Desktop) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kIsolateOrigins, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "https://example.com");
}

TEST_F(IsolateOriginsPolicyHandlerTest,
       ApplyPolicySettings_DesktopIgnoreAndroid) {
  SetPolicy("IsolateOriginsAndroid", base::Value("https://example.com"));
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kIsolateOrigins, nullptr));
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_ValidDesktop) {
  SetPolicy(key::kIsolateOrigins, base::Value("https://example.com"));
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

TEST_F(IsolateOriginsPolicyHandlerTest, CheckPolicySettings_InvalidDesktop) {
  SetPolicy(key::kIsolateOrigins, base::Value(123));
  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace policy
