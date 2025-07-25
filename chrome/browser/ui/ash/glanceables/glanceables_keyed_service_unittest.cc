// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kPrimaryProfileName[] = "primary_profile@example.com";
const char kSecondaryProfileName[] = "secondary_profile@example.com";

}  // namespace

class GlanceablesKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  GlanceablesKeyedServiceTest()
      : scoped_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  TestingProfile* CreateProfile() override {
    const auto account_id = AccountId::FromUserEmail(kPrimaryProfileName);
    fake_chrome_user_manager()->AddUser(account_id);
    fake_chrome_user_manager()->LoginUser(account_id);
    session_controller_client()->AddUserSession(kPrimaryProfileName);
    session_controller_client()->SwitchActiveUser(account_id);
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    profile_prefs_ = prefs.get();
    return profile_manager()->CreateTestingProfile(
        kPrimaryProfileName, std::move(prefs), u"Test profile", /*avatar_id=*/0,
        TestingProfile::TestingFactories(), /*is_supervised_profile=*/false,
        /*is_new_profile=*/absl::nullopt, /*policy_service=*/absl::nullopt,
        /*is_main_profile=*/true);
  }

  FakeChromeUserManager* fake_chrome_user_manager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  TestSessionControllerClient* session_controller_client() {
    return ash_test_helper()->test_session_controller_client();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  // Pointer to the primary profile (returned by |profile()|) prefs - owned by
  // the profile.
  raw_ptr<sync_preferences::TestingPrefServiceSyncable,
          DanglingUntriaged | ExperimentalAsh>
      profile_prefs_ = nullptr;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(GlanceablesKeyedServiceTest, RegistersClientsInAsh) {
  auto* const controller = Shell::Get()->glanceables_controller();
  EXPECT_FALSE(controller->GetClassroomClient());
  EXPECT_FALSE(controller->GetTasksClient());

  auto service = std::make_unique<GlanceablesKeyedService>(profile());
  EXPECT_TRUE(controller->GetClassroomClient());
  EXPECT_TRUE(controller->GetTasksClient());

  service->Shutdown();
  EXPECT_FALSE(controller->GetClassroomClient());
  EXPECT_FALSE(controller->GetTasksClient());
}

TEST_F(GlanceablesKeyedServiceTest, RegisterClientsInAshForNonPrimaryUser) {
  auto* const controller = Shell::Get()->glanceables_controller();
  auto service = std::make_unique<GlanceablesKeyedService>(profile());
  auto* const classroom_client_primary = controller->GetClassroomClient();
  auto* const tasks_client_primary = controller->GetTasksClient();
  EXPECT_TRUE(classroom_client_primary);
  EXPECT_TRUE(tasks_client_primary);

  const auto first_account_id = AccountId::FromUserEmail(kPrimaryProfileName);
  const auto second_account_id =
      AccountId::FromUserEmail(kSecondaryProfileName);
  fake_chrome_user_manager()->AddUser(second_account_id);
  fake_chrome_user_manager()->LoginUser(second_account_id);
  auto* secondary_profile =
      profile_manager()->CreateTestingProfile(kSecondaryProfileName,
                                              /*is_main_profile=*/false);
  session_controller_client()->AddUserSession(kSecondaryProfileName);
  auto service_secondary =
      std::make_unique<GlanceablesKeyedService>(secondary_profile);
  session_controller_client()->SwitchActiveUser(second_account_id);

  auto* const classroom_client_secondary = controller->GetClassroomClient();
  auto* const tasks_client_secondary = controller->GetTasksClient();
  EXPECT_TRUE(classroom_client_secondary);
  EXPECT_TRUE(tasks_client_secondary);
  EXPECT_NE(classroom_client_primary, classroom_client_secondary);
  EXPECT_NE(tasks_client_primary, tasks_client_secondary);

  session_controller_client()->SwitchActiveUser(first_account_id);
  EXPECT_EQ(classroom_client_primary, controller->GetClassroomClient());
  EXPECT_EQ(tasks_client_primary, controller->GetTasksClient());
}

TEST_F(GlanceablesKeyedServiceTest,
       DoesNotRegisterClientsInAshForDisabledPref) {
  auto* const controller = Shell::Get()->glanceables_controller();
  EXPECT_FALSE(controller->GetClassroomClient());
  EXPECT_FALSE(controller->GetTasksClient());

  auto service = std::make_unique<GlanceablesKeyedService>(profile());
  profile_prefs_->SetBoolean(prefs::kGlanceablesEnabled, false);
  EXPECT_FALSE(controller->GetClassroomClient());
  EXPECT_FALSE(controller->GetTasksClient());

  service->Shutdown();
  EXPECT_FALSE(controller->GetClassroomClient());
  EXPECT_FALSE(controller->GetTasksClient());
}

}  // namespace ash
