// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::UnorderedElementsAreArray;

constexpr char kSyncConsent[] = "sync-consent";

const test::UIPath kOverviewDialog = {kSyncConsent,
                                      "syncConsentOverviewDialog"};
const test::UIPath kReviewSettingsCheckBox = {kSyncConsent,
                                              "reviewSettingsBox"};
const test::UIPath kAcceptButton = {kSyncConsent, "acceptButton"};
const test::UIPath kDeclineButton = {kSyncConsent, "declineButton"};

const test::UIPath kLacrosOverviewDialog = {kSyncConsent,
                                            "syncConsentLacrosOverviewDialog"};

const test::UIPath kLacrosAcceptButton = {kSyncConsent, "syncEverythingButton"};
const test::UIPath kLacrosManageButtonRegular = {kSyncConsent,
                                                 "manageButtonRegularUser"};
const test::UIPath kLacrosManageButtonMinor = {kSyncConsent,
                                               "manageButtonMinorUser"};
const test::UIPath kLacrosDeclineButton = {kSyncConsent, "declineLacrosButton"};
const test::UIPath kLacrosNextButton = {kSyncConsent, "nextButton"};

const test::UIPath kAppsSyncToggle = {kSyncConsent, "appsTogglebutton"};
const test::UIPath kSettingsSyncToggle = {kSyncConsent, "settingsTogglebutton"};
const test::UIPath kWifiSyncToggle = {kSyncConsent, "wifiTogglebutton"};
const test::UIPath kWallpaperSyncToggle = {kSyncConsent,
                                           "wallpaperTogglebutton"};

syncer::SyncUserSettings* GetSyncUserSettings() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return SyncServiceFactory::GetForProfile(profile)->GetUserSettings();
}

class ConsentRecordedWaiter
    : public SyncConsentScreen::SyncConsentScreenTestDelegate {
 public:
  ConsentRecordedWaiter() = default;

  void Wait() {
    if (!consent_description_strings_.empty())
      return;

    run_loop_.Run();
  }

  // SyncConsentScreen::SyncConsentScreenTestDelegate
  void OnConsentRecordedIds(SyncConsentScreen::ConsentGiven consent_given,
                            const std::vector<int>& consent_description,
                            int consent_confirmation) override {
    consent_given_ = consent_given;
    consent_description_ids_ = consent_description;
    consent_confirmation_id_ = consent_confirmation;
  }

  void OnConsentRecordedStrings(
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation) override {
    consent_description_strings_ = consent_description;
    consent_confirmation_string_ = consent_confirmation;

    // SyncConsentScreenHandler::SyncConsentScreenHandlerTestDelegate is
    // notified after SyncConsentScreen::SyncConsentScreenTestDelegate, so
    // this is the only place where we need to quit loop.
    run_loop_.Quit();
  }

  SyncConsentScreen::ConsentGiven consent_given_;
  std::vector<int> consent_description_ids_;
  int consent_confirmation_id_;

  ::login::StringList consent_description_strings_;
  std::string consent_confirmation_string_;

  base::RunLoop run_loop_;
};

std::string GetLocalizedConsentString(const int id) {
  std::vector<std::u16string> str_substitute;
  str_substitute.push_back(ui::GetChromeOSDeviceName());
  std::string sanitized_string =
      base::UTF16ToUTF8(base::ReplaceStringPlaceholders(
          l10n_util::GetStringUTF16(id), str_substitute, nullptr));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");

  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, ">", "&gt;");

  return sanitized_string;
}

class SyncConsentTest
    : public OobeBaseTest,
      public SyncConsentScreen::SyncConsentScreenExitTestDelegate {
 public:
  SyncConsentTest(const SyncConsentTest&) = delete;
  SyncConsentTest& operator=(const SyncConsentTest&) = delete;

  SyncConsentTest() { login_manager_mixin_.set_session_restore_enabled(); }
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    if (is_lacros_enabled_) {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_ARC_RESTRICTED,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION_WITH_ARC_RESTRICTED,
      };
    } else {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE_WITH_DEVICE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_SUBTITLE_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME_2,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_DESCRIPTION,
      };
    }

    if (is_minor_user_) {
      // In minor mode, decline and turn on button should be displayed.
      expected_consent_ids_.push_back(IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2);
      expected_consent_ids_.push_back(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC);
    } else {
      // In regular mdoe, `review later` checkbox and accept button should be
      // displayed.
      if (is_lacros_enabled_) {
        expected_consent_ids_.push_back(
            IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER_ARC_RESTRICTED);
      } else {
        expected_consent_ids_.push_back(
            IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER);
      }
      expected_consent_ids_.push_back(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE);
    }

    SyncConsentScreen::SetSyncConsentScreenExitTestDelegate(this);
    SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(false);
  }

  void TearDownOnMainThread() override {
    SyncConsentScreen::SetSyncConsentScreenExitTestDelegate(nullptr);

    OobeBaseTest::TearDownOnMainThread();
  }

  void SwitchLanguage(const std::string& language) {
    WelcomeScreen* welcome_screen =
        WizardController::default_controller()->GetScreen<WelcomeScreen>();
    welcome_screen->UpdateLanguageList();
    test::LanguageReloadObserver observer(welcome_screen);
    test::OobeJS().SelectElementInPath(language,
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void LoginAndWaitForSyncConsentScreen(bool is_known_capability) {
    login_manager_mixin_.LoginAsNewRegularUser();

    if (is_known_capability) {
      SetIsMinorUser(is_minor_user_);
    }

    // If the screen has already exited, don't try to show it again.
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          SyncConsentScreenView::kScreenId);
    }

    // Sync Consent screen may skip, so OobeScreenWaiter will not stop. Use
    // custom predicate instead.
    test::TestPredicateWaiter(
        base::BindRepeating(
            [](SyncConsentTest* self) {
              if (self->screen_exited_)
                return true;

              if (!LoginDisplayHost::default_host()->GetOobeUI())
                return false;

              return LoginDisplayHost::default_host()
                         ->GetOobeUI()
                         ->current_screen() == SyncConsentScreenView::kScreenId;
            },
            base::Unretained(this)))
        .Wait();

    ASSERT_EQ(is_lacros_enabled_, crosapi::browser_util::IsLacrosEnabled());
    ASSERT_EQ(is_lacros_enabled_,
              AccountAppsAvailability::IsArcAccountRestrictionsEnabled());
  }

  // Attempts to log in and show sync consent screen if it is not to be skipped.
  void LoginAndShowSyncConsentScreenWithCapability() {
    LoginAndWaitForSyncConsentScreen(true);
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
    if (!GetSyncConsentScreen()->IsProfileSyncDisabledByPolicyForTest() &&
        LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build)
      GetSyncConsentScreen()->OnStateChanged(nullptr);
  }

  void LoginToSyncConsentScreenWithUnknownCapability() {
    LoginAndWaitForSyncConsentScreen(false);
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
    GetSyncConsentScreen()->OnStateChanged(nullptr);
  }

 protected:
  absl::optional<SyncConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;
  std::vector<int> expected_consent_ids_;
  bool is_minor_user_ = false;
  bool is_lacros_enabled_ = false;

  static SyncConsentScreen* GetSyncConsentScreen() {
    return static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));
  }

  std::vector<std::string> GetLocalizedExpectedConsentStrings() const {
    std::vector<std::string> result;
    for (const int& id : expected_consent_ids_) {
      result.push_back(GetLocalizedConsentString(id));
    }
    return result;
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // SyncConsentScreen::SyncConsentScreenExitTestDelegate
  void OnSyncConsentScreenExit(
      SyncConsentScreen::Result result,
      SyncConsentScreen::ScreenExitCallback& original_callback) override {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  void SetIsMinorUser(bool is_minor_user) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByGaiaId(test::kTestGaiaId);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_offer_extended_chrome_sync_promos(!is_minor_user);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  std::unique_ptr<base::AutoReset<bool>> force_branded_build_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedNotBrandedBuild) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      false;

  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->defer_oobe_flow_finished_for_tests = true;

  LoginAndShowSyncConsentScreenWithCapability();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedSyncDisabledByPolicy) {
  // Set up screen and policy.
  SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(true);

  LoginAndShowSyncConsentScreenWithCapability();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, PRE_AbortedSetup) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, AbortedSetup) {
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Expect sync everything toggle is on when consent flow is abandoned without
  // user action.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_TRUE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->IsSyncAllOsTypesEnabled());
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SyncConsentRecorder) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectHiddenPath(kDeclineButton);
  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Accept and continue",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);
}

// Tests the different combinations of minor mode and ARC restricted mode.
class SyncConsentTestWithModesParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SyncConsentTestWithModesParams() {
    std::tie(is_minor_user_, is_lacros_enabled_) = GetParam();
    if (is_lacros_enabled_) {
      // Make sure that `crosapi::browser_util::IsLacrosEnabled()` returns
      // `true`.
      scoped_feature_list_.InitWithFeatures(
          /*enabled=*/ash::standalone_browser::GetFeatureRefs(),
          /*disabled=*/{features::kOsSyncConsentRevamp});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled=*/{},
          /*disabled=*/ash::standalone_browser::GetFeatureRefs());
    }
  }

  SyncConsentTestWithModesParams(const SyncConsentTestWithModesParams&) =
      delete;
  SyncConsentTestWithModesParams& operator=(
      const SyncConsentTestWithModesParams&) = delete;

  ~SyncConsentTestWithModesParams() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithModesParams, Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);

  if (is_minor_user_) {
    test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);
  } else {
    test::OobeJS().ExpectVisiblePath(kReviewSettingsCheckBox);
  }

  test::OobeJS().TapOnPath(kAcceptButton);

  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));

  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestWithModesParams,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests the different combinations of Lacros state and the selection of
// the `Review later` checkbox.
class SyncConsentTestWithReviewParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SyncConsentTestWithReviewParams() {
    std::tie(is_lacros_enabled_, is_review_settings_checked_) = GetParam();
    if (is_lacros_enabled_) {
      // Make sure that `crosapi::browser_util::IsLacrosEnabled()` returns
      // `true`.
      scoped_feature_list_.InitWithFeatures(
          /*enabled=*/ash::standalone_browser::GetFeatureRefs(),
          /*disabled=*/{features::kOsSyncConsentRevamp});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled=*/{},
          /*disabled=*/ash::standalone_browser::GetFeatureRefs());
    }
  }

  SyncConsentTestWithReviewParams(const SyncConsentTestWithReviewParams&) =
      delete;
  SyncConsentTestWithReviewParams& operator=(
      const SyncConsentTestWithReviewParams&) = delete;

  ~SyncConsentTestWithReviewParams() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool is_review_settings_checked_;
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithReviewParams, Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  if (is_review_settings_checked_)
    test::OobeJS().TapOnPath(kReviewSettingsCheckBox);
  test::OobeJS().TapOnPath(kAcceptButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.ReviewFollowingSetup",
      is_review_settings_checked_, 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestWithReviewParams,
                         testing::Combine(testing::Bool(), testing::Bool()));

class SyncConsentTestWithParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  SyncConsentTestWithParams() = default;

  SyncConsentTestWithParams(const SyncConsentTestWithParams&) = delete;
  SyncConsentTestWithParams& operator=(const SyncConsentTestWithParams&) =
      delete;

  ~SyncConsentTestWithParams() override = default;
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams, SyncConsentTestWithLocale) {
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  SwitchLanguage(GetParam());
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_THAT(consent_recorded_waiter.consent_confirmation_string_,
              Eq(GetLocalizedConsentString(
                  IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE)));
}

// "es" tests language switching, "en-GB" checks switching to language varants.
INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestWithParams,
                         testing::Values("es", "en-GB"));

// Check that policy-disabled sync does not trigger SyncConsent screen.
//
// We need to check that "disabled by policy" disables SyncConsent screen
// independently from sync engine statis. So we run test twice, both for "sync
// engine not yet initialized" and "sync engine initialized" cases. Therefore
// we use WithParamInterface<bool> here.
class SyncConsentPolicyDisabledTest : public SyncConsentTest,
                                      public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(SyncConsentPolicyDisabledTest,
                       SyncConsentPolicyDisabled) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();

  OobeScreenExitWaiter waiter(SyncConsentScreenView::kScreenId);

  SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(true);
  SyncConsentScreen::SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(All, SyncConsentPolicyDisabledTest, testing::Bool());

// Tests that the SyncConsent screen performs a timezone request so that
// subsequent screens can have a timezone to work with, and that the timezone
// is properly stored in a preference.
class SyncConsentTimezoneOverride : public SyncConsentTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOobeTimezoneOverrideForTests,
                                    "TimezonePropagationTest");
    SyncConsentTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SyncConsentTimezoneOverride, MakesTimezoneRequest) {
  LoginAndShowSyncConsentScreenWithCapability();
  EXPECT_EQ("TimezonePropagationTest",
            g_browser_process->local_state()->GetString(
                ::prefs::kSigninScreenTimezone));
}

class SyncConsentMinorModeTest : public SyncConsentTest {
 public:
  SyncConsentMinorModeTest() { is_minor_user_ = true; }
  ~SyncConsentMinorModeTest() override = default;

 private:
  base::test::ScopedFeatureList sync_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, Accept) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kDeclineButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  test::OobeJS().TapOnPath(kAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  // Expect sync everything toggle is on after user accepted sync consent.
  EXPECT_TRUE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->IsSyncAllOsTypesEnabled());

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Turn on sync",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", true, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kAccepted, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       true, 1);
}

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, Decline) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kAcceptButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  test::OobeJS().TapOnPath(kDeclineButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  // Expect all data types are still disabled.
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());

  EXPECT_EQ(SyncConsentScreen::CONSENT_NOT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("No thanks", consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_DECLINE2,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.Behavior",
      SyncConsentScreen::SyncScreenBehavior::kShow, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", true, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.UserChoice",
      SyncConsentScreenHandler::UserChoice::kDeclined, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.SyncEnabled",
                                       false, 1);
}

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, PRE_AbortedSetup) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();
  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
}

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest, AbortedSetup) {
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_NE(profile, nullptr);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_NE(identity_manager, nullptr);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Expect all data types are disabled when consent flow is abandoned without
  // user action.
  syncer::SyncUserSettings* settings = GetSyncUserSettings();
  ASSERT_NE(settings, nullptr);
  EXPECT_FALSE(settings->IsSyncEverythingEnabled());
  EXPECT_TRUE(settings->GetSelectedTypes().Empty());
  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_TRUE(settings->GetSelectedOsTypes().Empty());
}

IN_PROC_BROWSER_TEST_F(SyncConsentMinorModeTest,
                       AssumeMinorUserWhenUnknownCapability) {
  LoginToSyncConsentScreenWithUnknownCapability();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kDeclineButton);
  test::OobeJS().ExpectHiddenPath(kReviewSettingsCheckBox);

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.IsCapabilityKnown", false, 1);
  histogram_tester_.ExpectUniqueSample("OOBE.SyncConsentScreen.IsMinorUser",
                                       true, 1);
}

class SyncConsentTimeoutTest : public SyncConsentTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeTriggerSyncTimeoutForTests);
    SyncConsentTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SyncConsentTimeoutTest,
                       SyncEngineInitializationTimeout) {
  auto overviewDialogWaiter =
      test::OobeJS().CreateVisibilityWaiter(true, {kOverviewDialog});

  LoginAndWaitForSyncConsentScreen(true);
  WaitForScreenShown();

  overviewDialogWaiter->Wait();
}

class SyncConsentLacrosRevampTest : public SyncConsentTest {
 public:
  SyncConsentLacrosRevampTest() {
    // Make sure that `crosapi::browser_util::IsLacrosEnabled()` returns `true`.
    auto features = ash::standalone_browser::GetFeatureRefs();
    features.push_back(features::kOsSyncConsentRevamp);
    sync_feature_list_.InitWithFeatures(
        /*enabled=*/features, /*disabled=*/{});
    is_lacros_enabled_ = true;
  }
  ~SyncConsentLacrosRevampTest() override = default;

  void SetLocalizedExpectedConsentStrings() {
    expected_consent_ids_.clear();
    expected_consent_ids_ = {
        IDS_LOGIN_OS_SYNC_CONSENT_SCREEN_TITLE,
        IDS_LOGIN_OS_SYNC_CONSENT_SCREEN_SUBTITLE,
        IDS_LOGIN_OS_SYNC_CONSENT_SCREEN_ADDITIONAL_SUBTITLE,
        IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC,
        IDS_LOGIN_OS_SYNC_CONSENT_SCREEN_SYNC_OPTIONS,
    };
  }

  void SetMinorMode() { is_minor_user_ = true; }

 private:
  base::test::ScopedFeatureList sync_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncConsentLacrosRevampTest, TurnOnSync) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SetLocalizedExpectedConsentStrings();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kLacrosOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kLacrosManageButtonRegular);
  test::OobeJS().TapOnPath(kLacrosAcceptButton);
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Turn on sync",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_TURN_ON_SYNC,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", true, 1);

  EXPECT_TRUE(settings->IsSyncAllOsTypesEnabled());
}

IN_PROC_BROWSER_TEST_F(SyncConsentLacrosRevampTest, SkippedSyncByPolicy) {
  // Set up screen and policy.
  SyncConsentScreen::SetProfileSyncDisabledByPolicyForTesting(true);

  LoginAndShowSyncConsentScreenWithCapability();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
}

IN_PROC_BROWSER_TEST_F(SyncConsentLacrosRevampTest, OnManage) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SetLocalizedExpectedConsentStrings();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kLacrosOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kLacrosAcceptButton);
  test::OobeJS().TapOnPath(kLacrosManageButtonRegular);
  test::OobeJS().TapOnPath(kLacrosNextButton);

  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_strings_,
              UnorderedElementsAreArray(GetLocalizedExpectedConsentStrings()));
  EXPECT_EQ("Manage sync options",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              UnorderedElementsAreArray(expected_consent_ids_));
  EXPECT_EQ(IDS_LOGIN_OS_SYNC_CONSENT_SCREEN_SYNC_OPTIONS,
            consent_recorded_waiter.consent_confirmation_id_);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", false, 1);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();

  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
}

IN_PROC_BROWSER_TEST_F(SyncConsentLacrosRevampTest, OnDecline) {
  SetMinorMode();
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kLacrosOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kLacrosAcceptButton);
  test::OobeJS().ExpectVisiblePath(kLacrosManageButtonMinor);
  test::OobeJS().TapOnPath(kLacrosDeclineButton);

  screen->SetDelegateForTesting(nullptr);  // cleanup

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::DECLINE);

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.SyncEverything", false, 1);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();

  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
}

class SyncConsentTestLacrosRevampWithParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  SyncConsentTestLacrosRevampWithParams() {
    std::tie(is_app_synced, is_settings_synced, is_wifi_synced,
             is_wallpaper_synced) = GetParam();
    // Make sure that `crosapi::browser_util::IsLacrosEnabled()` returns `true`.
    auto features = ash::standalone_browser::GetFeatureRefs();
    features.push_back(features::kOsSyncConsentRevamp);
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/features, /*disabled=*/{});
    is_lacros_enabled_ = true;
  }

  SyncConsentTestLacrosRevampWithParams(
      const SyncConsentTestLacrosRevampWithParams&) = delete;
  SyncConsentTestLacrosRevampWithParams& operator=(
      const SyncConsentTestLacrosRevampWithParams&) = delete;

  ~SyncConsentTestLacrosRevampWithParams() override = default;

 protected:
  bool is_app_synced;
  bool is_settings_synced;
  bool is_wifi_synced;
  bool is_wallpaper_synced;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentTestLacrosRevampWithParams,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

IN_PROC_BROWSER_TEST_P(SyncConsentTestLacrosRevampWithParams, ManageSync) {
  LoginAndShowSyncConsentScreenWithCapability();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, {kSyncConsent})->Wait();
  test::OobeJS().ExpectVisiblePath(kLacrosOverviewDialog);
  test::OobeJS().ExpectVisiblePath(kLacrosAcceptButton);
  test::OobeJS().TapOnPath(kLacrosManageButtonRegular);

  // check Toggle are enabled by default
  test::OobeJS().ExpectAttributeEQ("checked", kAppsSyncToggle, true);
  test::OobeJS().ExpectAttributeEQ("checked", kSettingsSyncToggle, true);
  test::OobeJS().ExpectAttributeEQ("checked", kWifiSyncToggle, true);
  test::OobeJS().ExpectAttributeEQ("checked", kWallpaperSyncToggle, true);

  // disable Toggle
  if (!is_app_synced) {
    test::OobeJS().TapOnPath(kAppsSyncToggle);
  }

  if (!is_settings_synced) {
    test::OobeJS().TapOnPath(kSettingsSyncToggle);
  }

  if (!is_wifi_synced) {
    test::OobeJS().TapOnPath(kWifiSyncToggle);
  }

  if (!is_wallpaper_synced) {
    test::OobeJS().TapOnPath(kWallpaperSyncToggle);
  }

  // check Toggle reflect Param
  test::OobeJS().ExpectAttributeEQ("checked", kAppsSyncToggle, is_app_synced);
  test::OobeJS().ExpectAttributeEQ("checked", kSettingsSyncToggle,
                                   is_settings_synced);
  test::OobeJS().ExpectAttributeEQ("checked", kWifiSyncToggle, is_wifi_synced);

  // Wallpaper sync is a special case; its implementation relies upon
  // OS Settings to be synced. Thus, the wallpaper label and toggle are
  // only enabled when the Settings sync toggle is on.
  test::OobeJS().ExpectAttributeEQ("checked", kWallpaperSyncToggle,
                                   is_wallpaper_synced && is_settings_synced);

  test::OobeJS().TapOnPath(kLacrosNextButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);

  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncWallpaper",
      is_wallpaper_synced && is_settings_synced, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncApps",
      is_app_synced, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncSettings",
      is_settings_synced, 1);
  histogram_tester_.ExpectUniqueSample(
      "OOBE.SyncConsentScreen.LacrosSyncOptIns.DataType.SyncWifi",
      is_wifi_synced, 1);

  // Expect all data types are disabled for minor users when initialized.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  syncer::SyncUserSettings* settings = GetSyncUserSettings();

  EXPECT_FALSE(settings->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(
      settings->GetSelectedOsTypes().Has(syncer::UserSelectableOsType::kOsApps),
      is_app_synced);
  EXPECT_EQ(settings->GetSelectedOsTypes().Has(
                syncer::UserSelectableOsType::kOsPreferences),
            is_settings_synced);
  EXPECT_EQ(settings->GetSelectedOsTypes().Has(
                syncer::UserSelectableOsType::kOsWifiConfigurations),
            is_wifi_synced);

  EXPECT_EQ(profile->GetPrefs()->GetBoolean(settings::prefs::kSyncOsWallpaper),
            is_wallpaper_synced && is_settings_synced);
}

}  // namespace
}  // namespace ash
