// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class NtpAndroidCustomBackgroundServiceTest : public testing::Test {
 protected:
  NtpAndroidCustomBackgroundServiceTest() = default;

  void SetUp() override {
    NtpAndroidCustomBackgroundService::RegisterProfilePrefs(
        pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(NtpAndroidCustomBackgroundServiceTest, PrefsRegistered) {
  EXPECT_TRUE(
      pref_service_.FindPreference(prefs::kNtpAndroidCustomBackgroundDict));
  EXPECT_TRUE(pref_service_.FindPreference(
      prefs::kNtpAndroidCustomBackgroundLocalToDevice));
}
