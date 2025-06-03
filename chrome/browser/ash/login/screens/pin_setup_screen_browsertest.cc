// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

constexpr char kPinSetupScreen[] = "pin-setup";

const test::UIPath kPinSetupScreenDoneStep = {kPinSetupScreen, "doneDialog"};

const test::UIPath kBackButton = {kPinSetupScreen, "backButton"};
const test::UIPath kNextButton = {kPinSetupScreen, "nextButton"};
const test::UIPath kSkipButton = {kPinSetupScreen, "setupSkipButton"};
const test::UIPath kDoneButton = {kPinSetupScreen, "doneButton"};
const test::UIPath kPinKeyboardInput = {kPinSetupScreen, "pinKeyboard",
                                        "pinKeyboard", "pinInput"};

}  // namespace

// Fixture to prepare oobe and the PIN setup screen. By default with this
// fixture, the PIN setup screen shouldn't be shown. There are child classes
// which perform the necessary setup so that the PIN setup screen is shown.
class PinSetupScreenTest : public OobeBaseTest {
 public:
  PinSetupScreenTest() { UserDataAuthClient::InitializeFake(); }

  ~PinSetupScreenTest() override = default;

  // This must be called very early (e.g. in the constructor) so that the
  // hardware support flag before `PinSetupScreen` reads it.
  static void SetHardwareSupport(bool is_supported) {
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(is_supported);
  }

  static void SetTabletMode(bool in_tablet_mode) {
    ShellTestApi().SetTabletModeEnabledForTest(in_tablet_mode);
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &PinSetupScreenTest::HandleScreenExit, base::Unretained(this)));

    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();

    // Force the sync screen to be shown so that we don't jump to PIN setup
    // screen (consuming auth session) in unbranded build
    wizard_context->is_branded_build = true;

    login_manager_mixin_.LoginAsNewRegularUser();

    // Add an authenticated session to the user context used during OOBE. In
    // production, this is set by earlier screens which are skipped in this
    // test.
    std::unique_ptr<UserContext> context;
    if (ash::features::ShouldUseAuthSessionStorage()) {
      context = ash::AuthSessionStorage::Get()->BorrowForTests(
          FROM_HERE, wizard_context->extra_factors_token.value());
    } else {
      context = std::move(wizard_context->extra_factors_auth_session);
    }
    // LoginManagerMixin uses StubAuthenticator that fills out authsession.
    // Reset Authsession to correctly interact with FakeUserDataAuthClient.
    context->ResetAuthSessionIds();
    cryptohome_.MarkUserAsExisting(context->GetAccountId());
    auto session_ids =
        cryptohome_.AddSession(context->GetAccountId(), /*authenticated=*/true);
    context->SetAuthSessionIds(session_ids.first, session_ids.second);
    if (ash::features::ShouldUseAuthSessionStorage()) {
      ash::AuthSessionStorage::Get()->Return(
          wizard_context->extra_factors_token.value(), std::move(context));
    } else {
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->extra_factors_auth_session = std::move(context);
    }
  }

  PinSetupScreen* GetScreen() {
    return WizardController::default_controller()->GetScreen<PinSetupScreen>();
  }

  void EnterPin() { test::OobeJS().TypeIntoPath("654321", kPinKeyboardInput); }

  void ShowPinSetupScreen() {
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          PinSetupScreenView::kScreenId);
    }
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(PinSetupScreenView::kScreenId).Wait();
  }

  void TapSkipButton() { test::OobeJS().TapOnPath(kSkipButton); }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ConfigureUserContextForTest() {
    if (ash::features::ShouldUseAuthSessionStorage()) {
      std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
      context->SetAuthSessionIds("fake-session-id", "broadcast");
      auto token = ash::AuthSessionStorage::Get()->Store(std::move(context));
      LoginDisplayHost::default_host()
          ->GetWizardContextForTesting()
          ->extra_factors_token = token;
    } else {
      LoginDisplayHost::default_host()
          ->GetWizardContextForTesting()
          ->extra_factors_auth_session = std::make_unique<UserContext>();
    }
  }

  void CheckCredentialsWereCleared() {
    if (ash::features::ShouldUseAuthSessionStorage()) {
      EXPECT_FALSE(LoginDisplayHost::default_host()
                       ->GetWizardContextForTesting()
                       ->extra_factors_token.has_value());
    } else {
      EXPECT_EQ(LoginDisplayHost::default_host()
                    ->GetWizardContextForTesting()
                    ->extra_factors_auth_session,
                nullptr);
    }
  }

  absl::optional<PinSetupScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;
  bool screen_exited_ = false;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};

 private:
  void HandleScreenExit(PinSetupScreen::Result result) {
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  PinSetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

// By default, oobe should skip the PIN setup screen.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, Skipped) {
  ShowPinSetupScreen();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::NOT_APPLICABLE);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 0);
}

// If the PIN setup screen is skipped, `extra_factors_auth_session` should be
// cleared.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, SkippedClearsAuthSession) {
  ConfigureUserContextForTest();

  ShowPinSetupScreen();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::NOT_APPLICABLE);
  CheckCredentialsWereCleared();
}

// Oobe should show the PIN setup screen if the device is in tablet mode.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShowInTabletMode) {
  SetTabletMode(true);

  ShowPinSetupScreen();
  TapSkipButton();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
}

// If the PIN setup screen is shown, `extra_factors_auth_session` should be
// cleared.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShowClearsAuthSession) {
  ConfigureUserContextForTest();
  SetTabletMode(true);

  ShowPinSetupScreen();
  TapSkipButton();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
  CheckCredentialsWereCleared();
}

// Fixture to pretend that we have hardware support for login.
class PinSetupScreenTestLoginSupport : public PinSetupScreenTest {
 public:
  PinSetupScreenTestLoginSupport() { SetHardwareSupport(true); }

  ~PinSetupScreenTestLoginSupport() override = default;
};

// Oobe should show the PIN setup screen if the TPM supports PIN for login.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport,
                       ShowWithHardwareSupport) {
  ShowPinSetupScreen();
  WaitForScreenShown();
  test::OobeJS().TapOnPath(kSkipButton);
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
}

// Oobe should skip the PIN setup screen if policies are set such that PIN
// cannot be used for both login/unlock and web authn.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, NoPinPolicy) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetList(prefs::kQuickUnlockModeAllowlist, base::Value::List());
  prefs->SetList(prefs::kWebAuthnFactors, base::Value::List());

  ShowPinSetupScreen();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::NOT_APPLICABLE);
}

// Oobe should show the PIN setup screen if the unlock factor policy allows
// PIN.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, PinForUnlockPolicy) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  base::Value::List unlock_factors;
  unlock_factors.Append(base::Value("PIN"));
  prefs->SetList(prefs::kQuickUnlockModeAllowlist, std::move(unlock_factors));
  prefs->SetList(prefs::kWebAuthnFactors, base::Value::List());

  ShowPinSetupScreen();
  TapSkipButton();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
}

// Oobe should show the PIN setup screen if the web authn factor policy allows
// PIN.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, PinForWebAuthnPolicy) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetList(prefs::kQuickUnlockModeAllowlist, base::Value::List());
  base::Value::List web_authn_factors;
  web_authn_factors.Append(base::Value("all"));
  prefs->SetList(prefs::kWebAuthnFactors, std::move(web_authn_factors));

  ShowPinSetupScreen();
  TapSkipButton();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
}

// A series of test cases for skipping the PIN setup page at various stages in
// the flow.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, SkipOnStart) {
  ShowPinSetupScreen();
  WaitForScreenShown();
  TapSkipButton();
  WaitForScreenExit();

  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Skipped", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(
              PinSetupScreen::UserAction::kSkipButtonClickedOnStart),
          1)));
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, SkipInFlow) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

  TapSkipButton();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Skipped", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(
              PinSetupScreen::UserAction::kSkipButtonClickedInFlow),
          1)));
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestLoginSupport, FinishedFlow) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kPinSetupScreenDoneStep})
      ->Wait();

  test::OobeJS().TapOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::DONE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(PinSetupScreen::UserAction::kDoneButtonClicked),
          1)));
}

}  // namespace ash
