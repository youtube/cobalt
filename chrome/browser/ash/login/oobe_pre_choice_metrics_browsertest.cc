// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_login_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/cros_pre_choice_metrics_manager.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_test_utils.h"
#include "components/session_manager/core/session_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

class OobePreChoiceMetricsTest : public OobeBaseTest {
 public:
  OobePreChoiceMetricsTest() {
    // TODO(crbug.com/396450575): Remove this line after ash feature flag is set
    // to true by default.
    feature_list_.InitAndEnableFeature(ash::features::kOobePreConsentMetrics);
  }

  OobePreChoiceMetricsTest(const OobePreChoiceMetricsTest&) = delete;
  OobePreChoiceMetricsTest& operator=(const OobePreChoiceMetricsTest&) = delete;
  ~OobePreChoiceMetricsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    OobeBaseTest::SetUp();
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    metrics::CrOSPreChoiceMetricsManager::Get()->SetCompletedPathForTesting(
        GetTestPath());
  }

  void TearDown() override { OobeBaseTest::TearDown(); }

  base::FilePath GetTestPath() {
    return temp_dir_.GetPath().Append("test-file");
  }

  void EnsureManagerSetupAndValidateChoice() {
    ASSERT_NE(metrics::CrOSPreChoiceMetricsManager::Get(), nullptr);

    ValidateMetricsConsent(/*enabled=*/true);
  }

  // Checks if the file that marks the end of pre-choice stage exists.
  void CheckForMarkerFile(base::Location from_here = FROM_HERE) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(GetTestPath()));
  }

  void ValidateMetricsConsent(bool enabled) {
    EXPECT_EQ(g_browser_process->local_state()->GetBoolean(
                  metrics::prefs::kMetricsReportingEnabled),
              enabled);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
  std::unique_ptr<metrics::CrOSPreChoiceMetricsManager> manager_;
};

IN_PROC_BROWSER_TEST_F(OobePreChoiceMetricsTest, RegularUserConsented) {
  EnsureManagerSetupAndValidateChoice();

  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  test::WaitForConsolidatedConsentScreen();

  test::TapConsolidatedConsentAccept();
  OobeScreenExitWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  // Wait for the background file-writing task to finish
  content::RunAllTasksUntilIdle();

  ValidateMetricsConsent(/*enabled=*/true);
  CheckForMarkerFile();
}

IN_PROC_BROWSER_TEST_F(OobePreChoiceMetricsTest, RegularUserDissented) {
  EnsureManagerSetupAndValidateChoice();

  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  test::WaitForConsolidatedConsentScreen();

  // Dissent to metrics. This will toggle the metrics setting, disabling
  // metrics.
  test::OobeJS().TapOnPath({"consolidated-consent", "usageOptin"});
  test::TapConsolidatedConsentAccept();
  OobeScreenExitWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  // Wait for the background file-writing task to finish
  content::RunAllTasksUntilIdle();

  ValidateMetricsConsent(/*enabled=*/false);
  CheckForMarkerFile();
}

IN_PROC_BROWSER_TEST_F(OobePreChoiceMetricsTest, GuestUserConsented) {
  EnsureManagerSetupAndValidateChoice();

  WizardController::default_controller()->AdvanceToScreen(
      GuestTosScreenView::kScreenId);
  OobeScreenWaiter(GuestTosScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"guest-tos", "overview"})
      ->Wait();

  // Accept guest tos since usage optin is default on.
  test::OobeJS().ClickOnPath({"guest-tos", "acceptButton"});

  // Wait for the background file-writing task to finish
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      ash::prefs::kOobeGuestMetricsEnabled));
  CheckForMarkerFile();

  // Validate pre-choice stage has completed and consent remains true before
  // first non-guest user sign in.
  session_manager::SessionManager::Get()->RequestSignOut();

  ValidateMetricsConsent(/*enabled=*/true);
  CheckForMarkerFile();
}

IN_PROC_BROWSER_TEST_F(OobePreChoiceMetricsTest, GuestUserDissented) {
  EnsureManagerSetupAndValidateChoice();

  WizardController::default_controller()->AdvanceToScreen(
      GuestTosScreenView::kScreenId);
  OobeScreenWaiter(GuestTosScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"guest-tos", "overview"})
      ->Wait();

  // Toggle usage optin off.
  test::OobeJS().ClickOnPath({"guest-tos", "usageOptin"});
  test::OobeJS().ClickOnPath({"guest-tos", "acceptButton"});

  // Wait for the background file-writing task to finish
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      ash::prefs::kOobeGuestMetricsEnabled));
  CheckForMarkerFile();

  // Validate pre-choice stage has completed and consent remains true before
  // first non-guest user sign in.
  session_manager::SessionManager::Get()->RequestSignOut();

  ValidateMetricsConsent(/*enabled=*/true);
  CheckForMarkerFile();
}

class ManagedOobePreChoiceMetricsTest : public OobePreChoiceMetricsTest {
 public:
  ManagedOobePreChoiceMetricsTest() = default;
  ~ManagedOobePreChoiceMetricsTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobePreChoiceMetricsTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(ManagedOobePreChoiceMetricsTest, ManagedUser) {
  EnsureManagerSetupAndValidateChoice();

  ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
  login_manager_mixin_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  ValidateMetricsConsent(/*enabled=*/true);
  EXPECT_FALSE(
      metrics::CrOSPreChoiceMetricsManager::Get()->is_enabled_for_testing());
  CheckForMarkerFile();
}

class ChildOobePreChoiceMetricsTest : public OobePreChoiceMetricsTest {
 public:
  ChildOobePreChoiceMetricsTest() = default;
  ~ChildOobePreChoiceMetricsTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    OobePreChoiceMetricsTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(ChildOobePreChoiceMetricsTest, ChildUser) {
  EnsureManagerSetupAndValidateChoice();

  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;

  login_manager_mixin_.LoginAsNewChildUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      ConsolidatedConsentScreenView::kScreenId);
  test::WaitForConsolidatedConsentScreen();

  test::TapConsolidatedConsentAccept();
  OobeScreenExitWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();

  ValidateMetricsConsent(/*enabled=*/true);
  CheckForMarkerFile();
}

class EnrolledDeviceOobePreChoiceMetricsTest : public OobePreChoiceMetricsTest {
 public:
  EnrolledDeviceOobePreChoiceMetricsTest() {
    LOG(WARNING) << "EnrolledDeviceOobePreChoiceMetricsTest";
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

// Since the device is already enrolled, pre-choice feature should not be
// enabled.
IN_PROC_BROWSER_TEST_F(EnrolledDeviceOobePreChoiceMetricsTest,
                       ShouldNotHavePreChoiceMetrics) {
  EXPECT_EQ(metrics::CrOSPreChoiceMetricsManager::Get(), nullptr);
  // It's unable to check the pre-choice complete file since the location of
  // the file, "/home/chronos", is not accessible by browser test. And because
  // CrOSPreChoiceMetricsManager is not created at all in
  // `CrOSPreChoiceMetricsManager::MaybeCreate` so it's not possible to
  // override the complete file location. However, by checking if the
  // CrOSPreChoiceMetricsManager is nullptr should be sufficient because it
  // indicates that the manager is not initialized and pre-choice will have no
  // chance to be enabled.
}

}  // namespace ash
