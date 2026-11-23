// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class MultistepFilterRetentionPrefsTest : public testing::Test {
 public:
  MultistepFilterRetentionPrefsTest() {
    RegisterRetentionProfilePrefs(prefs_.registry());
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(MultistepFilterRetentionPrefsTest, DefaultStateHasZeroCounts) {
  RetentionStateSnapshot state = GetRetentionState(&prefs_);
  EXPECT_EQ(state.suggestion_impressions, 0);
  EXPECT_EQ(state.suggestion_acceptances, 0);
  EXPECT_FALSE(state.is_last_suggestion_accepted);
}

TEST_F(MultistepFilterRetentionPrefsTest, RecordAcceptedOutcome) {
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kAccepted);

  RetentionStateSnapshot state = GetRetentionState(&prefs_);
  EXPECT_EQ(state.suggestion_impressions, 1);
  EXPECT_EQ(state.suggestion_acceptances, 1);
  EXPECT_TRUE(state.is_last_suggestion_accepted);
}

TEST_F(MultistepFilterRetentionPrefsTest, RecordRejectedOutcome) {
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kDismissed);

  RetentionStateSnapshot state = GetRetentionState(&prefs_);
  EXPECT_EQ(state.suggestion_impressions, 1);
  EXPECT_EQ(state.suggestion_acceptances, 0);
  EXPECT_FALSE(state.is_last_suggestion_accepted);
}

TEST_F(MultistepFilterRetentionPrefsTest,
       ImpressionDoesNotOverwriteLastAcceptance) {
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kAccepted);
  EXPECT_TRUE(GetRetentionState(&prefs_).is_last_suggestion_accepted);

  RecordImpression(&prefs_);
  EXPECT_TRUE(GetRetentionState(&prefs_).is_last_suggestion_accepted);
  EXPECT_EQ(GetRetentionState(&prefs_).suggestion_impressions, 2);
}

TEST_F(MultistepFilterRetentionPrefsTest, AccumulatesValuesSequentially) {
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kAccepted);
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kIgnored);
  RecordImpression(&prefs_);
  RecordUserInteraction(&prefs_, SuggestionUserDecision::kAccepted);

  RetentionStateSnapshot state = GetRetentionState(&prefs_);
  EXPECT_EQ(state.suggestion_impressions, 3);
  EXPECT_EQ(state.suggestion_acceptances, 2);
  EXPECT_TRUE(state.is_last_suggestion_accepted);
}

}  // namespace multistep_filter
