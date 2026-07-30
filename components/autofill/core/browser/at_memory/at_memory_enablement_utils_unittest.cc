// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/mock_personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::Return;

class AtMemoryEnablementUtilsTest : public testing::Test {
 protected:
  AtMemoryEnablementUtilsTest() {
    personal_context::prefs::RegisterProfilePrefs(pref_service_.registry());
    // Enable the toggle by default in tests since it represents the default
    // active state.
    pref_service_.SetUserPref(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::Value(true));
    // Set PersonalContextService to return not eligible by default.
    ON_CALL(personal_context_service_, GetEnablementState)
        .WillByDefault(Return(personal_context::PersonalContextEnablementState::
                                  kDisabledNotEligible));
  }

  base::test::ScopedFeatureList feature_list_{features::kAutofillAtMemory};
  testing::NiceMock<personal_context::MockPersonalContextEnablementService>
      personal_context_service_;
  TestingPrefServiceSimple pref_service_;
};

// Tests that `MayPerformAtMemoryAction` returns false when AtMemory is
// disabled.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_AtMemoryDisabled) {
  base::test::ScopedFeatureList disabled_features;
  disabled_features.InitAndDisableFeature(features::kAutofillAtMemory);
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_,
                                        &pref_service_));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                        &personal_context_service_,
                                        &pref_service_));
  EXPECT_FALSE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               &personal_context_service_, &pref_service_));
}

// Tests that `MayPerformAtMemoryAction` returns false when
// `personal_context_service` is null.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_NullPersonalContextService) {
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        nullptr, &pref_service_));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                        nullptr, &pref_service_));
  EXPECT_FALSE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               nullptr, &pref_service_));
}

// Tests `MayPerformAtMemoryAction` when `pref_service` is null.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_NullPrefService) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_, nullptr));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       &personal_context_service_, nullptr));
  EXPECT_FALSE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               &personal_context_service_, nullptr));
}

// Tests `MayPerformAtMemoryAction` under various Personal Context states.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_States) {
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));

  // State: kEnabled
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  EXPECT_TRUE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               &personal_context_service_, &pref_service_));

  // State: kEnabledShouldShowNotice
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kEnabledShouldShowNotice));
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  EXPECT_TRUE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               &personal_context_service_, &pref_service_));

  // State: kDisabledViaPersonalIntelligenceInAutofillToggle
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledViaPersonalIntelligenceInAutofillToggle));
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       &personal_context_service_,
                                       &pref_service_));

  // State: kDisabledNeedsOptIn
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledNeedsOptIn));
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       &personal_context_service_,
                                       &pref_service_));

  // State: kDisabledNotEligible
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillOnce(Return(personal_context::PersonalContextEnablementState::
                           kDisabledNotEligible));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_,
                                        &pref_service_));
}

// Tests `MayPerformAtMemoryAction` when the toggle pref is off.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_ToggleOff) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));

  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_,
                                        &pref_service_));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                                       &personal_context_service_,
                                       &pref_service_));
  EXPECT_FALSE(
      MayPerformAtMemoryAction(AtMemoryAction::kAllowCustomizeAtMemoryShortcut,
                               &personal_context_service_, &pref_service_));
}

// Tests that `MayPerformAtMemoryAction returns false when
// `personal_context_service` returns `kDisabledNotEligible`.
TEST_F(AtMemoryEnablementUtilsTest, MayPerformAtMemoryAction_NotSupported) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(Return(personal_context::PersonalContextEnablementState::
                                 kDisabledNotEligible));
  EXPECT_FALSE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                        &personal_context_service_,
                                        &pref_service_));
}

// Tests that `MayPerformAtMemoryAction` returns true when the client supports
// AtMemory and the settings toggle is enabled.
TEST_F(AtMemoryEnablementUtilsTest,
       MayPerformAtMemoryAction_SupportedAndToggleOn) {
  EXPECT_CALL(personal_context_service_, GetEnablementState)
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  pref_service_.SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  EXPECT_TRUE(MayPerformAtMemoryAction(AtMemoryAction::kTriggerSearchUI,
                                       &personal_context_service_,
                                       &pref_service_));
}

}  // namespace
}  // namespace autofill
