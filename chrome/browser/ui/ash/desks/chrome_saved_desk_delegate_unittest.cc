// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_saved_desk_delegate.h"

#include "ash/public/cpp/ash_public_export.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {
constexpr char kTestProfileEmail[] = "test@test.com";
}  // namespace

class ChromeSavedDeskDelegateTest : public testing::Test {
 public:
  ChromeSavedDeskDelegateTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {}

  ChromeSavedDeskDelegateTest(const ChromeSavedDeskDelegateTest&) = delete;
  ChromeSavedDeskDelegateTest& operator=(const ChromeSavedDeskDelegateTest&) =
      delete;

  ~ChromeSavedDeskDelegateTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Create a test user and profile so the `ChromeSavedDeskDelegate` does not
    // return empty result simply because of missing user profile.
    auto account_id = AccountId::FromUserEmail(kTestProfileEmail);
    const auto* user = GetFakeUserManager()->AddUser(account_id);

    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kTestProfileEmail);
    profile_builder.SetPath(profile_dir_.GetPath());
    profile_ = profile_builder.Build();

    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());

    chrome_saved_desk_delegate_ = std::make_unique<ChromeSavedDeskDelegate>();
  }

  void TearDown() override {
    chrome_saved_desk_delegate_.reset();
    profile_.reset();
    profile_manager_.reset();
  }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  ChromeSavedDeskDelegate* chrome_saved_desk_delegate() {
    return chrome_saved_desk_delegate_.get();
  }

  full_restore::FullRestoreSaveHandler* GetSaveHandler(
      bool start_save_timer = true) {
    auto* save_handler = full_restore::FullRestoreSaveHandler::GetInstance();
    save_handler->SetActiveProfilePath(profile_->GetPath());
    save_handler->AllowSave();
    return save_handler;
  }

 private:
  // Browser profiles need to be created on UI thread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  base::ScopedTempDir profile_dir_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<ChromeSavedDeskDelegate> chrome_saved_desk_delegate_;

  user_manager::ScopedUserManager user_manager_enabler_;
};

TEST_F(ChromeSavedDeskDelegateTest, NullWindowReturnsEmptyAppLaunchData) {
  base::test::TestFuture<std::unique_ptr<app_restore::AppLaunchInfo>> future;
  chrome_saved_desk_delegate()->GetAppLaunchDataForSavedDesk(
      /*window=*/nullptr, future.GetCallback());
  auto app_launch_info = future.Take();
  EXPECT_FALSE(app_launch_info);
}
