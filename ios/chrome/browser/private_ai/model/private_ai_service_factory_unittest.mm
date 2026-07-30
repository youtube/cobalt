// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/private_ai/model/private_ai_service_factory.h"

#import <optional>
#import <string_view>

#import "base/test/scoped_feature_list.h"
#import "components/private_ai/features.h"
#import "components/private_ai/private_ai_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace private_ai {

class PrivateAiServiceFactoryTest : public PlatformTest {
 protected:
  PrivateAiServiceFactoryTest() {
    profile_ = TestProfileIOS::Builder().Build();
  }

  TestProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the PrivateAiService is successfully created when the Private AI
// feature flag is enabled.
TEST_F(PrivateAiServiceFactoryTest, ServiceCreatedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});

  PrivateAiService* service = PrivateAiServiceFactory::GetForProfile(profile());
  EXPECT_NE(service, nullptr);
}

// Tests that the PrivateAiService is null when the Private AI feature flag is
// disabled.
TEST_F(PrivateAiServiceFactoryTest, ServiceNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPrivateAi);

  PrivateAiService* service = PrivateAiServiceFactory::GetForProfile(profile());
  EXPECT_EQ(service, nullptr);
}

// Tests that the PrivateAiService is always null for Incognito (Off-the-Record)
// profiles, regardless of the feature flag state.
TEST_F(PrivateAiServiceFactoryTest, ServiceNullForIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});

  ProfileIOS* otr_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  EXPECT_EQ(PrivateAiServiceFactory::GetForProfile(otr_profile), nullptr);
}

}  // namespace private_ai
