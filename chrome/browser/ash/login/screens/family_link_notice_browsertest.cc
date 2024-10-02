// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

const test::UIPath kFamilyLinkDialog = {"family-link-notice",
                                        "familyLinkDialog"};
const test::UIPath kContinueButton = {"family-link-notice", "continueButton"};

}  // namespace

class FamilyLinkNoticeScreenTest : public OobeBaseTest {
 public:
  FamilyLinkNoticeScreenTest() = default;
  ~FamilyLinkNoticeScreenTest() override = default;

  void SetUpOnMainThread() override {
    FamilyLinkNoticeScreen* screen = static_cast<FamilyLinkNoticeScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            FamilyLinkNoticeView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &FamilyLinkNoticeScreenTest::HandleScreenExit, base::Unretained(this)));
    OobeBaseTest::SetUpOnMainThread();
  }

  void LoginAsRegularUser() {
    login_manager_mixin_.LoginAsNewRegularUser();
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
  }

  void ExpectHelpAppPrefValue(bool expected) {
    EXPECT_TRUE(help_app_pref_fal_.has_value());
    EXPECT_EQ(help_app_pref_fal_.value(), expected);
  }

  void ClickContinueButtonOnFamilyLinkScreen() {
    test::OobeJS().ExpectVisiblePath(kFamilyLinkDialog);
    test::OobeJS().ExpectVisiblePath(kContinueButton);
    test::OobeJS().TapOnPath(kContinueButton);
  }

  void WaitForScreenExit() {
    if (screen_result_.has_value())
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  absl::optional<FamilyLinkNoticeScreen::Result> screen_result_;

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};

 private:
  void HandleScreenExit(FamilyLinkNoticeScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;

    // Fetch the values before OOBE is eventually destroyed after the exit
    // callback.
    WizardController::default_controller()->PrepareFirstRunPrefs();
    help_app_pref_fal_ =
        ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
            prefs::kHelpAppShouldShowParentalControl);

    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  absl::optional<bool> help_app_pref_fal_;
  base::RepeatingClosure screen_exit_callback_;
  FamilyLinkNoticeScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};

  base::test::ScopedFeatureList feature_list_;
};

// Verify that regular account user should not see family link notice screen
// after log in.
IN_PROC_BROWSER_TEST_F(FamilyLinkNoticeScreenTest, RegularAccount) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = false;
  LoginAsRegularUser();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), FamilyLinkNoticeScreen::Result::SKIPPED);
  ExpectHelpAppPrefValue(false);
}

// Verify user should see family link notice screen when selecting to sign in
// as a child account but log in as a regular account.
IN_PROC_BROWSER_TEST_F(FamilyLinkNoticeScreenTest, NonSupervisedChildAccount) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  LoginAsRegularUser();
  OobeScreenWaiter(FamilyLinkNoticeView::kScreenId).Wait();
  ClickContinueButtonOnFamilyLinkScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), FamilyLinkNoticeScreen::Result::DONE);
  ExpectHelpAppPrefValue(true);
}

class FamilyLinkNoticeScreenChildTest : public FamilyLinkNoticeScreenTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Child users require a user policy, set up an empty one so the user can
    // get through login.
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void LoginAsChildUser() {
    login_manager_mixin_.LoginAsNewChildUser();
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
  }

 private:
  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId),
      &policy_server_mixin_};
};

// Verify child account user should not see family link notice screen after log
// in.
IN_PROC_BROWSER_TEST_F(FamilyLinkNoticeScreenChildTest, ChildAccount) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  LoginAsChildUser();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), FamilyLinkNoticeScreen::Result::SKIPPED);
  ExpectHelpAppPrefValue(false);
}

// Verify child account user should not see family link notice screen after log
// in if not selecting sign in as child.
IN_PROC_BROWSER_TEST_F(FamilyLinkNoticeScreenChildTest,
                       ChildAccountSignInAsRegular) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = false;
  LoginAsChildUser();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), FamilyLinkNoticeScreen::Result::SKIPPED);
  ExpectHelpAppPrefValue(false);
}

class FamilyLinkNoticeScreenManagedTest : public FamilyLinkNoticeScreenTest {
 public:
  void LoginAsManagedUser() {
    user_policy_mixin_.RequestPolicyUpdate();
    login_manager_mixin_.LoginWithDefaultContext(test_user_);
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
  }

 private:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(FamilyLinkNoticeScreenManagedTest, ManagedAccount) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  LoginAsManagedUser();
  OobeScreenWaiter(FamilyLinkNoticeView::kScreenId).Wait();
  ClickContinueButtonOnFamilyLinkScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), FamilyLinkNoticeScreen::Result::DONE);
  ExpectHelpAppPrefValue(false);
}

}  // namespace ash
