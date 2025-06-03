// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDownloadConnectorEnabledNonBlockingPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["malware"]}
    ]
  }
])";

constexpr char kDownloadConnectorEnabledBlockingPref[] = R"([
  {
    "service_provider": "google",
    "block_until_verdict":1,
    "enable": [
      {"url_list": ["*"], "tags": ["malware"]}
    ]
  }
])";

}  // namespace

namespace download {

class DownloadBubblePrefsTest : public testing::Test {
 public:
  DownloadBubblePrefsTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadBubblePrefsTest(const DownloadBubblePrefsTest&) = delete;
  DownloadBubblePrefsTest& operator=(const DownloadBubblePrefsTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
    profile_->GetPrefs()->SetInteger(
        ConnectorScopePref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        policy::POLICY_SCOPE_MACHINE);
  }

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  base::test::ScopedFeatureList feature_list_;

  void ExpectFeatureFlagEnabledStatus(bool expect_enabled) {
    bool is_enabled = IsDownloadBubbleEnabled(profile_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_FALSE(is_enabled);
#else
    EXPECT_EQ(is_enabled, expect_enabled);
#endif
  }

  void ExpectFeatureFlagV2EnabledStatus(bool expect_enabled) {
    bool is_enabled = IsDownloadBubbleV2Enabled(profile_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_FALSE(is_enabled);
#else
    EXPECT_EQ(is_enabled, expect_enabled);
#endif
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadBubblePrefsTest, FeatureFlagEnabled) {
  feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/true);
}

TEST_F(DownloadBubblePrefsTest, FeatureFlagDisabled) {
  feature_list_.InitAndDisableFeature(safe_browsing::kDownloadBubble);
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/false);
}

TEST_F(DownloadBubblePrefsTest, DoesDownloadConnectorBlock) {
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
  profile_->GetPrefs()->Set(
      enterprise_connectors::ConnectorPref(
          enterprise_connectors::FILE_DOWNLOADED),
      *base::JSONReader::Read(kDownloadConnectorEnabledNonBlockingPref));
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
  profile_->GetPrefs()->Set(
      enterprise_connectors::ConnectorPref(
          enterprise_connectors::FILE_DOWNLOADED),
      *base::JSONReader::Read(kDownloadConnectorEnabledBlockingPref));
  EXPECT_TRUE(DoesDownloadConnectorBlock(profile_, GURL()));
}

TEST_F(DownloadBubblePrefsTest, V2FeatureFlagEnabled) {
  feature_list_.InitWithFeatures(
      {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/true);
  ExpectFeatureFlagV2EnabledStatus(/*expect_enabled=*/true);
}

TEST_F(DownloadBubblePrefsTest, V2FeatureFlagDisabled_YesMVP_NoV2) {
  feature_list_.InitWithFeatures({safe_browsing::kDownloadBubble},
                                 {safe_browsing::kDownloadBubbleV2});
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/true);
  ExpectFeatureFlagV2EnabledStatus(/*expect_enabled=*/false);
}

TEST_F(DownloadBubblePrefsTest, V2FeatureFlagDisabled_NoMVP_NoV2) {
  feature_list_.InitWithFeatures(
      {}, {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2});
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/false);
  ExpectFeatureFlagV2EnabledStatus(/*expect_enabled=*/false);
}

TEST_F(DownloadBubblePrefsTest, V2FeatureFlagDisabled_NoMVP_YesV2) {
  feature_list_.InitWithFeatures({safe_browsing::kDownloadBubbleV2},
                                 {safe_browsing::kDownloadBubble});
  ExpectFeatureFlagEnabledStatus(/*expect_enabled=*/false);
  ExpectFeatureFlagV2EnabledStatus(/*expect_enabled=*/false);
}

TEST_F(DownloadBubblePrefsTest, ShouldSuppressIph) {
  // Test default value.
  EXPECT_FALSE(ShouldSuppressDownloadBubbleIph(profile_));

  // Test when user has previous interaction with the bubble.
  profile_->GetPrefs()->SetBoolean(prefs::kDownloadBubbleIphSuppression, true);
  EXPECT_TRUE(ShouldSuppressDownloadBubbleIph(profile_));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(DownloadBubblePrefsTest, IsPartialViewEnabled) {
  // Test default value.
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabled(profile_));
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));

  // Set value.
  SetDownloadBubblePartialViewEnabled(profile_, false);
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabled(profile_));
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));

  SetDownloadBubblePartialViewEnabled(profile_, true);
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabled(profile_));
  // This should still be false because it has been set to an explicit value.
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));
}
#else
TEST_F(DownloadBubblePrefsTest, IsPartialViewEnabled) {
  // Returns false regardless of the pref.
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabled(profile_));
  SetDownloadBubblePartialViewEnabled(profile_, true);
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabled(profile_));
  SetDownloadBubblePartialViewEnabled(profile_, false);
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabled(profile_));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(DownloadBubblePrefsTest, PartialViewImpressions) {
  // Test default value.
  EXPECT_EQ(DownloadBubblePartialViewImpressions(profile_), 0);

  // Set value.
  SetDownloadBubblePartialViewImpressions(profile_, 1);
  EXPECT_EQ(DownloadBubblePartialViewImpressions(profile_), 1);
}

}  // namespace download
