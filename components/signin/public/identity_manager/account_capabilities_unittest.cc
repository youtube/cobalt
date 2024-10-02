// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, CanHaveEmailAddressDisplayed) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_have_email_address_displayed(true);
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kTrue);

  mutator.set_can_have_email_address_displayed(false);
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanOfferExtendedChromeSyncPromos) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kTrue);

  mutator.set_can_offer_extended_chrome_sync_promos(false);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanRunChromePrivacySandboxTrials) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_run_chrome_privacy_sandbox_trials(true);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kTrue);

  mutator.set_can_run_chrome_privacy_sandbox_trials(false);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanStopParentalSupervision) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_stop_parental_supervision(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_stop_parental_supervision(true);
  EXPECT_EQ(capabilities.can_stop_parental_supervision(),
            signin::Tribool::kTrue);

  mutator.set_can_stop_parental_supervision(false);
  EXPECT_EQ(capabilities.can_stop_parental_supervision(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanToggleAutoUpdates) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_toggle_auto_updates(true);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kTrue);

  mutator.set_can_toggle_auto_updates(false);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsAllowedForMachineLearning) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_allowed_for_machine_learning(true);
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kTrue);

  mutator.set_is_allowed_for_machine_learning(false);
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsSubjectToEnterprisePolicies) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_enterprise_policies(true);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_enterprise_policies(false);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsSubjectToParentalControls) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_parental_controls(false);
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kFalse);
}

// Temporary test that should be modified once
// `is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice()` is
// based on a real account capability and not derived from other capabilities
// client-side.
//
// TODO(crbug.com/1430845): Update to a regular capabilities test once
// is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice()`
// is backed by a real account capability.
TEST_F(AccountCapabilitiesTest,
       IsSubjectToPrivacySandboxRestrictedMeasurementApiNotice) {
  {
    // `can_run_chrome_privacy_sandbox_trials` is unknown
    // `set_is_subject_to_parental_controls` is unknown
    AccountCapabilities c;
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kUnknown);
  }

  {
    // `can_run_chrome_privacy_sandbox_trials` is unknown
    AccountCapabilities c;
    AccountCapabilitiesTestMutator mutator(&c);
    mutator.set_is_subject_to_parental_controls(true);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kUnknown);
    mutator.set_is_subject_to_parental_controls(false);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kUnknown);
  }

  {
    // `can_run_chrome_privacy_sandbox_trials` is true
    AccountCapabilities c;
    AccountCapabilitiesTestMutator mutator(&c);
    mutator.set_can_run_chrome_privacy_sandbox_trials(true);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kUnknown);

    mutator.set_is_subject_to_parental_controls(true);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kFalse);
    mutator.set_is_subject_to_parental_controls(false);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kFalse);
  }

  {
    // `can_run_chrome_privacy_sandbox_trials` is false
    AccountCapabilities c;
    AccountCapabilitiesTestMutator mutator(&c);
    mutator.set_can_run_chrome_privacy_sandbox_trials(false);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kUnknown);

    mutator.set_is_subject_to_parental_controls(true);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kFalse);
    mutator.set_is_subject_to_parental_controls(false);
    EXPECT_EQ(
        c.is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
        signin::Tribool::kTrue);
  }
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_PartiallyFilled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Filled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.SetAllSupportedCapabilities(true);
  EXPECT_TRUE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_UnknownToKnown) {
  AccountCapabilities capabilities;

  AccountCapabilities other;
  AccountCapabilitiesTestMutator mutator(&other);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_KnownToUnknown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;

  EXPECT_FALSE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_OverwriteKnown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;
  AccountCapabilitiesTestMutator other_mutator(&other);
  other_mutator.set_can_offer_extended_chrome_sync_promos(false);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kFalse,
            capabilities.can_offer_extended_chrome_sync_promos());
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolTrue) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolFalse) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(false);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolUnknown) {
  AccountCapabilities capabilities;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

#endif
