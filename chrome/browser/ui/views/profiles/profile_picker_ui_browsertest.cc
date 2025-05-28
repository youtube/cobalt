// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://profile-picker/ WebUI page. They live here
// and not in the webui directory because they manipulate views.
namespace {
struct ProfilePickerTestParam {
  PixelTestParam pixel_test_param;
  bool use_multiple_profiles = false;
  // Requires `use_multiple_profiles` to be enabled.
  bool has_supervised_user = false;
  bool show_kite_for_supervised_users = false;
  // param to be removed when `kOutlineSilhouetteIcon` is enabled by default.
  bool outline_silhouette_icon = false;
  bool disallow_profile_creation = false;
  bool use_glic_version = false;
  bool no_glic_eligible_profiles = false;
  bool is_enterprise_badging_enabled = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like
// ProfilePickerUIPixelTest.InvokeUi_default/<TestSuffix>`
// instead of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfilePickerTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const ProfilePickerTestParam kTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "MultipleProfiles"},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "PortraitModeWindow",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize}},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesNoProfileCreation"},
     .use_multiple_profiles = true,
     .disallow_profile_creation = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_OutlineSilhouette"},
     .use_multiple_profiles = true,
     .outline_silhouette_icon = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix =
                              "DarkRtlSmallMultipleProfiles_OutlineSilhouette",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true,
     .outline_silhouette_icon = true},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_Kite"},
     .use_multiple_profiles = true,
     .has_supervised_user = true,
     .show_kite_for_supervised_users = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles_Kite",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true,
     .has_supervised_user = true,
     .show_kite_for_supervised_users = true},
    {.pixel_test_param = {.test_suffix = "ManagedProfileHasWorkLabel"},
     .use_multiple_profiles = true,
     .is_enterprise_badging_enabled = true},
#endif
    {.pixel_test_param = {.test_suffix = "GlicRegular"},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicRegularSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicRegularPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicNoProfiles"},
     .use_glic_version = true,
     .no_glic_eligible_profiles = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfiles"},
     .use_multiple_profiles = true,
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfilesSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true,
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfilesPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_multiple_profiles = true,
     .use_glic_version = true},

};

enum class ProfileStatus {
  kSignedOut,
  kSignedIn,
  kSignedInManaged,
  kSignedInSupervised,
};

// Make non empty account Glic eligible in Glic mode by adapting the
// account capabilities of the signed in account and propagating them to the
// `ProfileAttributesEntry`.
void EnableGlicForAccount(signin::IdentityManager* identity_manager,
                          AccountInfo account_info) {
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);

  // In order to have the propagation of the account capabilities in the
  // `ProfileAttributesEntry` the account info must be complete/valid.
  // Fill them with dummy data if not filled already.
  if (account_info.hosted_domain.empty()) {
    account_info.hosted_domain = signin::constants::kNoHostedDomainFound;
  }
  if (account_info.full_name.empty()) {
    account_info.full_name = "Joe Testing";
  }
  if (account_info.given_name.empty()) {
    account_info.given_name = "Joe";
  }
  if (account_info.picture_url.empty()) {
    account_info.picture_url = "PICTURE_URL_EMPTY";
  }
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

void SetSigninProfileProperties(signin::IdentityManager* identity_manager,
                                ProfileStatus profile_status,
                                bool is_glic_version) {
  CHECK(identity_manager);

  AccountInfo account_info;
  switch (profile_status) {
    case ProfileStatus::kSignedOut:
      break;
    case ProfileStatus::kSignedIn:
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joe@gmail.com", signin::ConsentLevel::kSignin);
      break;
    case ProfileStatus::kSignedInManaged: {
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joework@example.com",
          signin::ConsentLevel::kSignin);
      account_info =
          FillAccountInfo(account_info, AccountManagementStatus::kManaged,
                          signin::Tribool::kUnknown);
      signin::UpdateAccountInfoForAccount(identity_manager, account_info);
      break;
    }
    case ProfileStatus::kSignedInSupervised: {
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joejunior@gmail.com",
          signin::ConsentLevel::kSignin);
      supervised_user::UpdateSupervisionStatusForAccount(
          account_info, identity_manager, true);
      break;
    }
  }

  if (!account_info.IsEmpty() && is_glic_version) {
    EnableGlicForAccount(identity_manager, account_info);
  }
}

// Create 4 profiles with different icons and types.
void AddMultipleProfiles(bool is_glic_version, bool has_supervised_user) {
  std::vector<ProfileStatus> profiles_status;
  if (is_glic_version) {
    // For the glic version, we need all Profiles to be signed in.
    profiles_status.insert(
        profiles_status.end(),
        {ProfileStatus::kSignedIn, ProfileStatus::kSignedInManaged,
         ProfileStatus::kSignedIn, ProfileStatus::kSignedInManaged});
  } else {
    profiles_status.insert(profiles_status.end(),
                           {ProfileStatus::kSignedOut, ProfileStatus::kSignedIn,
                            ProfileStatus::kSignedInManaged});
    if (has_supervised_user) {
      profiles_status.push_back(ProfileStatus::kSignedInSupervised);
    }
  }

  size_t icon_index = 0;
  for (ProfileStatus profile_status : profiles_status) {
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"Joe", icon_index++, /*is_hidden=*/false,
        /*initialized_callback=*/
        base::BindLambdaForTesting(
            [&run_loop, &profile_status, &is_glic_version](Profile* profile) {
              SetSigninProfileProperties(
                  IdentityManagerFactory::GetForProfile(profile),
                  profile_status, is_glic_version);
              if (profile_status == ProfileStatus::kSignedInManaged) {
                enterprise_util::SetUserAcceptedAccountManagement(profile,
                                                                  true);
              }
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}
}  // namespace

class ProfilePickerUIPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<ProfilePickerTestParam> {
 public:
  ProfilePickerUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureStates(
        {{supervised_user::kShowKiteForSupervisedUsers,
          GetParam().show_kite_for_supervised_users},
         {kOutlineSilhouetteIcon, GetParam().outline_silhouette_icon},
         {features::kEnterpriseProfileBadgingForAvatar,
          GetParam().is_enterprise_badging_enabled}});
#endif
  }

  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    bool is_glic_version = GetParam().use_glic_version;
    bool no_glic_eligible_profiles = GetParam().no_glic_eligible_profiles;

    // In Glic mode, sign in the default account as well if we need eligible
    // profiles.
    if (is_glic_version && !no_glic_eligible_profiles) {
      SetSigninProfileProperties(
          IdentityManagerFactory::GetForProfile(browser()->profile()),
          ProfileStatus::kSignedIn,
          /*is_glic_version=*/true);
    }

    if (GetParam().use_multiple_profiles) {
      // In Glic mode, if `use_multiple_profiles` is set,
      // `no_glic_eligible_profiles` must be set to false.
      CHECK(!is_glic_version || !no_glic_eligible_profiles);
      AddMultipleProfiles(is_glic_version, GetParam().has_supervised_user);
    }

    if (GetParam().disallow_profile_creation) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kBrowserAddPersonEnabled, false);
    }

    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    GURL profile_picker_main_view_url = GURL(chrome::kChromeUIProfilePickerUrl);
    // Since we override the FlowController, we need to give in the full Url
    // with the glic query param from the start.
    if (is_glic_version) {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerGlicQuery);
      profile_picker_main_view_url =
          profile_picker_main_view_url.ReplaceComponents(replacements);
    }

    content::TestNavigationObserver observer(profile_picker_main_view_url);
    observer.StartWatchingNewWebContents();

    ProfilePicker::Params params =
        is_glic_version
            ? ProfilePicker::Params::ForGlicManager(base::DoNothing())
            // We use `ProfilePicker::Params::ForFirstRun` here because it is
            // the only constructor that lets us force a profile to use.
            : ProfilePicker::Params::ForFirstRun(
                  browser()->profile()->GetPath(), base::DoNothing());

    profile_picker_view_ = new ProfileManagementStepTestView(
        std::move(params),
        ProfileManagementFlowController::Step::kProfilePicker,
        /*step_controller_factory=*/
        base::BindLambdaForTesting(
            [profile_picker_main_view_url](ProfilePickerWebContentsHost* host) {
              return ProfileManagementStepController::CreateForProfilePickerApp(
                  host, profile_picker_main_view_url);
            }));
    profile_picker_view_->ShowAndWait(GetParam().pixel_test_param.window_size);
    observer.Wait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "ProfilePickerUIPixelTest", screenshot_name) !=
           ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
};

IN_PROC_BROWSER_TEST_P(ProfilePickerUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfilePickerUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
