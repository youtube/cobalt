// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/chromeos/app_mode/app_session_policies.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AppSessionPolociesTest : public testing::Test {
 public:
  AppSessionPolociesTest() {
    app_session_policies_ = std::make_unique<AppSessionPolicies>(GetPrefs());
  }

  AppSessionPolociesTest(const AppSessionPolociesTest&) = delete;
  AppSessionPolociesTest& operator=(const AppSessionPolociesTest&) = delete;

  void TearDown() override {
    // Clean up all preferenses that we use in tests.
    GetPrefs()->ClearPref(prefs::kNewWindowsInKioskAllowed);
  }

  bool IsWindowCreationAllowed() const {
    return app_session_policies_->IsWindowCreationAllowed();
  }

  PrefService* GetPrefs() { return profile_.GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  // |profile_| needs to outlive |app_session_policies_|.
  TestingProfile profile_;
  std::unique_ptr<AppSessionPolicies> app_session_policies_;
};

TEST_F(AppSessionPolociesTest, kNewWindowsInKioskAllowedNotSet) {
  EXPECT_FALSE(IsWindowCreationAllowed());
}

TEST_F(AppSessionPolociesTest, kNewWindowsInKioskAllowedSetFalse) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, false);
  EXPECT_FALSE(IsWindowCreationAllowed());
}

TEST_F(AppSessionPolociesTest, kNewWindowsInKioskAllowedSetTrue) {
  GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed, true);
  EXPECT_TRUE(IsWindowCreationAllowed());
}

}  // namespace chromeos
