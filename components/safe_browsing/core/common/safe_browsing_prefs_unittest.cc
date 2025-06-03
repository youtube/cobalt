// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingPrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
    prefs_.registry()->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingSawInterstitialScoutReporting, false);
    prefs_.registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_.registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.registry()->RegisterListPref(prefs::kSafeBrowsingAllowlistDomains);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kHashPrefixRealTimeChecksAllowedByPolicy, true);
  }

  void ResetPrefs(bool scout_reporting) {
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                      scout_reporting);
  }

  void ExpectPrefs(bool scout_reporting) {
    LOG(INFO) << "Pref values: scout=" << scout_reporting;
    EXPECT_EQ(scout_reporting,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  }

  void ExpectPrefsExist(bool scout_reporting) {
    LOG(INFO) << "Prefs exist: scout=" << scout_reporting;
    EXPECT_EQ(scout_reporting,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled));
  }
  TestingPrefServiceSimple prefs_;
};

TEST_F(SafeBrowsingPrefsTest,
       TailoredSecuritySyncFlowLastRunTimeIsInitialized) {
  TestingPrefServiceSimple prefs;
  RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(prefs.GetTime(prefs::kTailoredSecuritySyncFlowLastRunTime),
            base::Time());
}

TEST_F(SafeBrowsingPrefsTest,
       TailoredSecuritySyncLastUserInteractionStateIsInitialized) {
  TestingPrefServiceSimple prefs;
  RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(
      prefs.GetValue(prefs::kTailoredSecuritySyncFlowLastUserInteractionState),
      TailoredSecurityRetryState::UNSET);
}

TEST_F(SafeBrowsingPrefsTest,
       TailoredSecurityNextSyncFlowTimestampIsInitialized) {
  TestingPrefServiceSimple prefs;
  RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(prefs.GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp),
            base::Time());
}

TEST_F(SafeBrowsingPrefsTest, TailoredSecuritySyncFlowRetryStateIsInitialized) {
  TestingPrefServiceSimple prefs;
  RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(prefs.GetValue(prefs::kTailoredSecuritySyncFlowRetryState),
            TailoredSecurityRetryState::UNSET);
}

TEST_F(SafeBrowsingPrefsTest,
       TailoredSecuritySyncUserObservedOutcomeUnsetTimestampIsInitialized) {
  TestingPrefServiceSimple prefs;
  RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(prefs.GetTime(
                prefs::kTailoredSecuritySyncFlowObservedOutcomeUnsetTimestamp),
            base::Time());
}

TEST_F(SafeBrowsingPrefsTest, GetSafeBrowsingExtendedReportingLevel) {
  // By Default, extended reporting is off.
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));

  // The value of the Scout pref affects the reporting level directly.
  ResetPrefs(/*scout_reporting=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
  // Scout pref off, so reporting is off.
  ResetPrefs(/*scout_reporting=*/false);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyMatchesPasswordProtectionLoginURL) {
  GURL url("https://mydomain.com/login.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  base::Value::List login_urls;
  login_urls.Append("https://otherdomain.com/login.html");
  prefs_.SetList(prefs::kPasswordProtectionLoginURLs, login_urls.Clone());
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  login_urls.Append("https://mydomain.com/login.html");
  prefs_.SetList(prefs::kPasswordProtectionLoginURLs, std::move(login_urls));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_TRUE(MatchesPasswordProtectionLoginURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest,
       VerifyMatchesPasswordProtectionChangePasswordURL) {
  GURL url("https://mydomain.com/change_password.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://otherdomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_TRUE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest, EnhancedProtection) {
  // Confirm default state.
  EXPECT_FALSE(IsEnhancedProtectionEnabled(prefs_));

  SetEnhancedProtectionPrefForTests(&prefs_, true);
  EXPECT_TRUE(IsEnhancedProtectionEnabled(prefs_));
  {
    prefs_.SetBoolean(prefs::kSafeBrowsingEnabled, false);
    EXPECT_FALSE(IsEnhancedProtectionEnabled(prefs_));
  }
}

TEST_F(SafeBrowsingPrefsTest, InitializesEsbProtegoPingWithTokenLastLogTime) {
  TestingPrefServiceSimple prefs;
  safe_browsing::RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(
      prefs.GetTime(prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime),
      base::Time());
}

TEST_F(SafeBrowsingPrefsTest,
       InitializesEsbProtegoPingWithoutTokenLastLogTime) {
  TestingPrefServiceSimple prefs;
  safe_browsing::RegisterProfilePrefs(prefs.registry());
  EXPECT_EQ(
      prefs.GetTime(prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime),
      base::Time());
}

TEST_F(SafeBrowsingPrefsTest, IsExtendedReportingPolicyManaged) {
  // This test checks that manipulating SBEROptInAllowed and the management
  // state of SBER behaves as expected. Below, we describe what should happen
  // to the results of IsExtendedReportingPolicyManaged and
  // IsExtendedReportingOptInAllowed.

  // Confirm default state, SBER should be disabled, OptInAllowed should
  // be enabled, and SBER is not managed.
  EXPECT_FALSE(IsExtendedReportingEnabled(prefs_));
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Setting SBEROptInAllowed to false disallows opt-in but doesn't change
  // whether SBER is managed.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  EXPECT_FALSE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));
  // Setting the value back to true reverts back to the default.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Make the SBER pref managed and enable it and ensure that the pref gets
  // the expected value. Making SBER managed doesn't change the
  // SBEROptInAllowed setting.
  prefs_.SetManagedPref(prefs::kSafeBrowsingScoutReportingEnabled,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs_.IsManagedPreference(prefs::kSafeBrowsingScoutReportingEnabled));
  // The value of the pref comes from the policy.
  EXPECT_TRUE(IsExtendedReportingEnabled(prefs_));
  // SBER being managed doesn't change the SBEROptInAllowed pref.
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyIsURLAllowlistedByPolicy) {
  GURL target_url("https://www.foo.com");

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kSafeBrowsingAllowlistDomains));
  base::Value::List allowlisted_domains;
  allowlisted_domains.Append("foo.com");
  prefs_.SetList(prefs::kSafeBrowsingAllowlistDomains,
                 std::move(allowlisted_domains));
  StringListPrefMember string_list_pref;
  string_list_pref.Init(prefs::kSafeBrowsingAllowlistDomains, &prefs_);
  EXPECT_TRUE(IsURLAllowlistedByPolicy(target_url, prefs_));

  GURL not_allowlisted_url("https://www.bar.com");
  EXPECT_FALSE(IsURLAllowlistedByPolicy(not_allowlisted_url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest,
       VerifyIsRealTimeDownloadProtectionRequestAllowed) {
  // Confirm default state.
  EXPECT_TRUE(IsRealTimeDownloadProtectionRequestAllowed(prefs_));
  prefs_.SetBoolean(prefs::kRealTimeDownloadProtectionRequestAllowedByPolicy,
                    false);
  EXPECT_FALSE(IsRealTimeDownloadProtectionRequestAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyIsCsdPhishingProtectionAllowed) {
  EXPECT_TRUE(IsCsdPhishingProtectionAllowed(prefs_));
  prefs_.SetBoolean(prefs::kSafeBrowsingCsdPhishingProtectionAllowedByPolicy,
                    false);
  EXPECT_FALSE(IsCsdPhishingProtectionAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifySafeBrowsingExtensionProtectionAllowed) {
  EXPECT_TRUE(IsSafeBrowsingExtensionProtectionAllowed(prefs_));
  prefs_.SetBoolean(prefs::kSafeBrowsingExtensionProtectionAllowedByPolicy,
                    false);
  EXPECT_FALSE(IsSafeBrowsingExtensionProtectionAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyHashPrefixRealTimeChecksAllowedByPolicy) {
  EXPECT_TRUE(AreHashPrefixRealTimeLookupsAllowedByPolicy(prefs_));
  prefs_.SetBoolean(prefs::kHashPrefixRealTimeChecksAllowedByPolicy, false);
  EXPECT_FALSE(AreHashPrefixRealTimeLookupsAllowedByPolicy(prefs_));
}
}  // namespace safe_browsing
