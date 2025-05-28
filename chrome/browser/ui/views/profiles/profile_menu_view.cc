// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/sync/sync_passphrase_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace {

constexpr int kBusinessIconPadding = 4;
constexpr int kBusinessIconSize =
    ProfileMenuViewBase::kManagementBadgeSize - 2 * kBusinessIconPadding;

// Helpers --------------------------------------------------------------------

std::u16string GetSyncErrorButtonText(AvatarSyncErrorType error) {
  switch (error) {
    case AvatarSyncErrorType::kSyncPaused:
    case AvatarSyncErrorType::kUnrecoverableError:
      // The user was signed out. Offer them to sign in again.
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON);
    case AvatarSyncErrorType::kManagedUserUnrecoverableError:
      // As opposed to the corresponding error in an unmanaged account
      // (AvatarSyncErrorType::kUnrecoverableError), sign-out hasn't happened
      // here yet. The button directs to the sign-out confirmation dialog in
      // settings.
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON);
    case AvatarSyncErrorType::kUpgradeClientError:
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON);
    case AvatarSyncErrorType::kPassphraseError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON);
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError:
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError:
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForEverythingError:
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForPasswordsError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_RETRIEVE_KEYS_BUTTON);
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
      return l10n_util::GetStringUTF16(
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
  }
}

std::u16string GetProfileIdentifier(const ProfileAttributesEntry& entry) {
  switch (entry.GetNameForm()) {
    case NameForm::kGaiaName:
    case NameForm::kLocalName:
      return entry.GetName();
    case NameForm::kGaiaAndLocalName:
      return l10n_util::GetStringFUTF16(
          IDS_PROFILE_MENU_PROFILE_IDENTIFIER_WITH_SEPARATOR,
          entry.GetGAIANameToDisplay(), entry.GetLocalProfileName());
  }
}

}  // namespace

// ProfileMenuView ---------------------------------------------------------

// static
bool ProfileMenuView::close_on_deactivate_for_testing_ = true;

ProfileMenuView::ProfileMenuView(views::Button* anchor_button, Browser* browser)
    : ProfileMenuViewBase(anchor_button, browser) {
  set_close_on_deactivate(close_on_deactivate_for_testing_);
}

ProfileMenuView::~ProfileMenuView() = default;

void ProfileMenuView::BuildMenu() {
  Profile* profile = browser()->profile();
  if (profile->IsGuestSession()) {
    BuildGuestIdentity();
    MaybeBuildCloseBrowsersButton();
    if (base::FeatureList::IsEnabled(
            switches::kEnableImprovedGuestProfileMenu)) {
      AddBottomMargin();
    }
    return;
  }

  CHECK(!profile->IsOffTheRecord());
  const bool is_web_app = web_app::AppBrowserController::IsWebApp(browser());
  if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
    SetMenuTitleForAccessibility();
    BuildIdentityWithCallToAction();
  } else {
    BuildIdentity();

    // Users should not be able to open chrome settings from WebApps.
    if (!is_web_app) {
      BuildSyncInfo();
      BuildAutofillButtons();
    }
  }

  if (is_web_app) {
    browser()->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  // Users should not be able to use features from WebApps.
  if (!is_web_app) {
    BuildFeatureButtons();
  }

  std::vector<ProfileAttributesEntry*> available_profiles;
  bool show_guest_in_other_profiles_section = false;
  GetProfilesForOtherProfilesSection(available_profiles,
                                     show_guest_in_other_profiles_section);

  if (!available_profiles.empty() || show_guest_in_other_profiles_section) {
    SetProfileManagementHeading(
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PROFILES_LIST_TITLE));
  }
  BuildOtherProfilesSection(available_profiles,
                            show_guest_in_other_profiles_section);
  base::UmaHistogramBoolean("ProfileChooser.HasProfilesShown",
                            !available_profiles.empty());

  // Users should not be able to manage profiles from WebApps.
  if (!is_web_app) {
    BuildProfileManagementFeatureButtons();
  }
}

gfx::ImageSkia ProfileMenuView::GetSyncIcon() const {
  Profile* profile = browser()->profile();
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    return gfx::ImageSkia();
  }

  bool is_sync_feature_enabled =
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSync);
  if (!is_sync_feature_enabled) {
    // This is done regardless of GetAvatarSyncErrorType() because the icon
    // should reflect that sync-the-feature is off. The error will still be
    // highlighted by other parts of the UI.
    return ColoredImageForMenu(kSyncDisabledChromeRefreshIcon,
                               kColorProfileMenuSyncOffIcon);
  }

  std::optional<AvatarSyncErrorType> error = GetAvatarSyncErrorType(profile);
  if (!error) {
    return ColoredImageForMenu(kSyncChromeRefreshIcon,
                               kColorProfileMenuSyncIcon);
  }

  ui::ColorId color_id = error == AvatarSyncErrorType::kSyncPaused
                             ? kColorProfileMenuSyncPausedIcon
                             : kColorProfileMenuSyncErrorIcon;
  return ColoredImageForMenu(kSyncDisabledChromeRefreshIcon, color_id);
}

std::u16string ProfileMenuView::GetAccessibleWindowTitle() const {
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);

  if (!menu_title_.empty()) {
    title = l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA, title,
                                       menu_title_);
  }
  if (!menu_subtitle_.empty()) {
    title = l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA, title,
                                       menu_subtitle_);
  }
  return title;
}

void ProfileMenuView::OnProfileManagementButtonClicked() {
  RecordClick(ActionableItem::kProfileManagementLabel);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ExecuteCommand(browser(), IDC_SHOW_MANAGEMENT_PAGE);
}

void ProfileMenuView::OnManageGoogleAccountButtonClicked() {
  RecordClick(ActionableItem::kManageGoogleAccountButton);
  if (!perform_menu_actions()) {
    return;
  }

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  NavigateToGoogleAccountPage(
      profile,
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

void ProfileMenuView::OnPasswordsButtonClicked() {
  RecordClick(ActionableItem::kPasswordsButton);
  if (!perform_menu_actions()) {
    return;
  }
  NavigateToManagePasswordsPage(
      browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
}

void ProfileMenuView::OnCreditCardsButtonClicked() {
  RecordClick(ActionableItem::kCreditCardsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
}

void ProfileMenuView::OnAddressesButtonClicked() {
  RecordClick(ActionableItem::kAddressesButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
}

void ProfileMenuView::OnGuestProfileButtonClicked() {
  RecordClick(ActionableItem::kGuestProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  DCHECK(profiles::IsGuestModeEnabled(*browser()->profile()));
  profiles::SwitchToGuestProfile();
}

void ProfileMenuView::OnExitProfileButtonClicked() {
  RecordClick(ActionableItem::kExitProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  profiles::CloseProfileWindows(browser()->profile());
}

void ProfileMenuView::OnSyncSettingsButtonClicked() {
  RecordClick(ActionableItem::kSyncSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
}

void ProfileMenuView::OnSyncErrorButtonClicked(AvatarSyncErrorType error) {
  RecordClick(ActionableItem::kSyncErrorButton);
  if (!perform_menu_actions()) {
    return;
  }

  // The logic below must be consistent with GetSyncInfoForAvatarErrorType().
  switch (error) {
    case AvatarSyncErrorType::kManagedUserUnrecoverableError:
      chrome::ShowSettingsSubPage(browser(), chrome::kSignOutSubPage);
      break;
    case AvatarSyncErrorType::kUnrecoverableError: {
      Profile* profile = browser()->profile();
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      // This error means that the Sync engine failed to initialize. Shutdown
      // Sync engine by revoking sync consent.
      identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
          signin_metrics::ProfileSignout::kUserClickedSignoutSettings);
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      // Re-enable sync with the same primary account.
      signin_ui_util::EnableSyncFromSingleAccountPromo(
          profile,
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          signin_metrics::AccessPoint::kAvatarBubbleSignIn);
      break;
    }
    case AvatarSyncErrorType::kSyncPaused:
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          browser()->profile(),
          signin_metrics::AccessPoint::kAvatarBubbleSignIn);
      break;
    case AvatarSyncErrorType::kUpgradeClientError:
      chrome::OpenUpdateChromeDialog(browser());
      break;
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForEverythingError:
    case AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError:
      OpenTabForSyncKeyRetrieval(
          browser(), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForEverythingError:
    case AvatarSyncErrorType::
        kTrustedVaultRecoverabilityDegradedForPasswordsError:
      OpenTabForSyncKeyRecoverabilityDegraded(
          browser(), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
      break;
    case AvatarSyncErrorType::kPassphraseError:
      if (base::FeatureList::IsEnabled(switches::kImprovedSigninUIOnDesktop)) {
        ShowSyncPassphraseDialog(
            *browser(), base::BindRepeating(
                            &SyncPassphraseDialogDecryptData,
                            base::Unretained(SyncServiceFactory::GetForProfile(
                                browser()->profile()))));
      } else {
        chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      }
      break;
    case AvatarSyncErrorType::kSettingsUnconfirmedError:
      chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
      break;
  }
}

void ProfileMenuView::OnSigninButtonClicked(
    CoreAccountInfo account,
    ActionableItem button_type,
    signin_metrics::AccessPoint access_point) {
  RecordClick(button_type);

  if (!perform_menu_actions()) {
    return;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);

  if (button_type == ActionableItem::kSigninReauthButton) {
    // The reauth button does not trigger a sync opt in.
    signin_ui_util::ShowReauthForAccount(browser()->profile(), account.email,
                                         access_point);
    return;
  }

  signin_ui_util::EnableSyncFromSingleAccountPromo(browser()->profile(),
                                                   account, access_point);
}

void ProfileMenuView::OnSignoutButtonClicked() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  DCHECK(ChromeSigninClientFactory::GetForProfile(browser()->profile())
             ->IsClearPrimaryAccountAllowed(identity_manager->HasPrimaryAccount(
                 signin::ConsentLevel::kSync)))
      << "Clear primary account is not allowed. Signout should not be offered "
         "in the UI.";

  RecordClick(ActionableItem::kSignoutButton);
  if (!perform_menu_actions()) {
    return;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  browser()->signin_view_controller()->SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt,
      signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
      signin_metrics::SourceForRefreshTokenOperation::
          kUserMenu_SignOutAllAccounts);
}

void ProfileMenuView::OnOtherProfileSelected(
    const base::FilePath& profile_path) {
  RecordClick(ActionableItem::kOtherProfileButton);
  if (!perform_menu_actions()) {
    return;
  }

  if (!web_app::AppBrowserController::IsWebApp(browser())) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    profiles::SwitchToProfile(profile_path, /*always_create=*/false);
  } else {
    // Open the same web app for another profile.
    // On non-macOS the only allowlisted case is PasswordManager WebApp, which
    // uses a different code path from other PWAs as it needs to not only
    // support switching profiles, but also possibly installing the app into a
    // different profile. Regular PWAs can only switch to profiles where the app
    // is already installed.
    const webapps::AppId& app_id = browser()->app_controller()->app_id();
#if BUILDFLAG(IS_MAC)
    if (app_id != ash::kPasswordManagerAppId) {
      apps::AppShimManager::Get()->LaunchAppInProfile(app_id, profile_path);
      return;
    }
#endif
    CHECK_EQ(app_id, ash::kPasswordManagerAppId);

    app_profile_switcher_.emplace(
        app_id, *browser()->profile(),
        base::BindOnce(
            [](views::Widget* widget) {
              widget->CloseWithReason(
                  views::Widget::ClosedReason::kUnspecified);
            },
            // It's safe to use base::Unretained, because the profile
            // switcher is owned by ProfileMenuView and is destroyed
            // before the widget is destroyed.
            base::Unretained(GetWidget())));
    app_profile_switcher_->SwitchToProfile(profile_path);
  }
}

void ProfileMenuView::OnAddNewProfileButtonClicked() {
  RecordClick(ActionableItem::kAddNewProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
}

void ProfileMenuView::OnManageProfilesButtonClicked() {
  RecordClick(ActionableItem::kManageProfilesButton);
  if (!perform_menu_actions()) {
    return;
  }
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
}

void ProfileMenuView::OnEditProfileButtonClicked() {
  RecordClick(ActionableItem::kEditProfileButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kManageProfileSubPage);
}

void ProfileMenuView::OnCookiesClearedOnExitLinkClicked() {
  RecordClick(ActionableItem::kCookiesClearedOnExitLink);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kContentSettingsSubPage +
                                             std::string("/") +
                                             chrome::kCookieSettingsSubPage);
}

void ProfileMenuView::OnAutofillSettingsButtonClicked() {
  RecordClick(ActionableItem::kAutofillSettingsButton);
  if (!perform_menu_actions()) {
    return;
  }
  chrome::ShowSettingsSubPage(browser(), chrome::kAutofillSubPage);
}

void ProfileMenuView::SetMenuTitleForAccessibility() {
  CHECK(switches::IsImprovedSigninUIOnDesktopEnabled());
  Profile* profile = browser()->profile();
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);

  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn: {
      std::string profile_user_display_name, profile_user_email;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
      profile_user_display_name = profile->GetPrefs()->GetString(
          enterprise_signin::prefs::kProfileUserDisplayName);
      profile_user_email = profile->GetPrefs()->GetString(
          enterprise_signin::prefs::kProfileUserEmail);
#endif
      menu_title_ =
          profile_user_display_name.empty()
              ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
              : base::UTF8ToUTF16(profile_user_display_name);
      menu_subtitle_ = base::UTF8ToUTF16(profile_user_email);  // Often empty.
    } break;
    case signin_util::SignedInState::kSyncPaused:
    case signin_util::SignedInState::kSignInPending:
      menu_title_ = base::UTF8ToUTF16(account_info.full_name);
      menu_subtitle_ =
          l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
      break;
    case signin_util::SignedInState::kSyncing:
    case signin_util::SignedInState::kSignedIn:
      menu_title_ = base::UTF8ToUTF16(account_info.full_name);
      menu_subtitle_ = base::UTF8ToUTF16(account_info.email);
      break;
  }

  if (GetWidget()) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void ProfileMenuView::BuildIdentity() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(account);
  ProfileAttributesEntry* profile_attributes =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!profile_attributes) {
    // May happen if the profile is being deleted. https://crbug.com/1040079
    return;
  }

  std::u16string profile_name;
  std::optional<EditButtonParams> edit_button_params;
  profile_name = profile_attributes->GetLocalProfileName();

  SkColor background_color =
      profile_attributes->GetProfileThemeColors().profile_highlight_color;
  if (!account_info.IsEmpty()) {
    menu_title_ = base::UTF8ToUTF16(account_info.full_name);
    menu_subtitle_ =
        IsSyncPaused(profile)
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(account_info.email);
    auto account_manager = GetAccountManagerIdentity(profile);
    std::u16string management_label;
    ui::ImageModel badge_image_model;

    if (enterprise_util::CanShowEnterpriseBadgingForMenu(
            browser()->profile())) {
      management_label =
          account_manager
              ? l10n_util::GetStringFUTF16(IDS_PROFILES_MANAGED_BY,
                                           base::UTF8ToUTF16(*account_manager))
              : std::u16string();
      if (auto* icon = policy::ManagementServiceFactory::GetForProfile(profile)
                           ->GetManagementIconForProfile()) {
        badge_image_model = *icon;
      } else {
        gfx::ImageSkia sized_icon = gfx::CreateVectorIcon(gfx::IconDescription(
            vector_icons::kBusinessIcon, kBusinessIconSize,
            browser()->window()->GetColorProvider()->GetColor(
                ui::kColorMenuIcon)));
        badge_image_model =
            ui::ImageModel::FromImageSkia(gfx::CanvasImageSource::CreatePadded(
                sized_icon, gfx::Insets(kBusinessIconPadding)));
      }
    }

    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(
            profile_attributes->GetProfileManagementOidcTokens()
                    .id_token.empty()
                ? account_info.account_image
                : profile_attributes->GetAvatarIcon(kIdentityImageSize)),
        badge_image_model, menu_title_, menu_subtitle_, management_label);
  } else {
    std::string profile_user_display_name, profile_user_email;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    profile_user_display_name = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserDisplayName);
    profile_user_email = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
#endif
    menu_title_ =
        profile_user_display_name.empty()
            ? l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE)
            : base::UTF8ToUTF16(profile_user_display_name);
    // The email may be empty.
    menu_subtitle_ = base::UTF8ToUTF16(
        profile_user_email.empty() ? account_info.email : profile_user_email);

    std::u16string management_label;
    SetProfileIdentityInfo(
        profile_name, background_color, edit_button_params,
        ui::ImageModel::FromImage(
            // If the user is in the web-only signed-in state in the
            // experimental version of the UNO model, use the account image in
            // the profile menu header.
            // If the account does not have an image or it's not available yet,
            // a grey silhouette will be used.
            // If UNO is disabled or enabled in the full version, or there is no
            // account, use the profile icon.
            !account_info.IsEmpty()
                ? account_info.account_image
                : profile_attributes->GetAvatarIcon(kIdentityImageSize)),
        ui::ImageModel(), menu_title_, menu_subtitle_, management_label);
  }

  if (GetWidget()) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void ProfileMenuView::BuildGuestIdentity() {
  menu_title_ = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  menu_subtitle_ = std::u16string();

  if (base::FeatureList::IsEnabled(switches::kEnableImprovedGuestProfileMenu)) {
    IdentitySectionParams params;
    params.title = menu_title_;
    params.profile_image_padding =
        std::nearbyint(kIdentityInfoImageSize * 0.25f);
    params.profile_image = profiles::GetGuestAvatar();
    SetProfileIdentityWithCallToAction(std::move(params));
  } else {
    int guest_window_count = BrowserList::GetGuestBrowserCount();
    if (guest_window_count > 1) {
      menu_subtitle_ = l10n_util::GetPluralStringFUTF16(
          IDS_GUEST_WINDOW_COUNT_MESSAGE, guest_window_count);
    }

    SetProfileIdentityInfo(
        /*profile_name=*/std::u16string(),
        /*profile_background_color=*/SK_ColorTRANSPARENT,
        /*edit_button_params=*/std::nullopt, profiles::GetGuestAvatar(),
        ui::ImageModel(), menu_title_, menu_subtitle_,
        /*management_label=*/std::u16string(), &kGuestMenuArtIcon);
  }

  if (GetWidget()) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void ProfileMenuView::BuildAutofillButtons() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  AddShortcutFeatureButton(
      vector_icons::kPasswordManagerIcon,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE),
      base::BindRepeating(&ProfileMenuView::OnPasswordsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      kCreditCardChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK),
      base::BindRepeating(&ProfileMenuView::OnCreditCardsButtonClicked,
                          base::Unretained(this)));

  AddShortcutFeatureButton(
      vector_icons::kLocationOnChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK),
      base::BindRepeating(&ProfileMenuView::OnAddressesButtonClicked,
                          base::Unretained(this)));
}

void ProfileMenuView::BuildSyncInfo() {
  // TODO(crbug.com/370473765): Delete this function after
  // `switches::IsImprovedSigninUIOnDesktopEnabled()` is launched.
  CHECK(!switches::IsImprovedSigninUIOnDesktopEnabled());

  Profile* profile = browser()->profile();
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return;
  }

  if (!SyncServiceFactory::IsSyncAllowed(profile)) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_sync_feature_enabled =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  // First, check for sync errors. They may exist even if sync-the-feature is
  // disabled and only sync-the-transport is running.
  const std::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile);
  if (error) {
    std::u16string error_description = GetAvatarSyncErrorDescription(
        *error, is_sync_feature_enabled,
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email);
    BuildSyncInfoWithCallToAction(
        std::move(error_description), GetSyncErrorButtonText(*error),
        base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                            base::Unretained(this), *error),
        /*show_sync_badge=*/is_sync_feature_enabled);
    return;
  }

  // If there's no error and sync-the-feature is enabled, the text says
  // everything is fine and the button simply opens sync settings.
  if (is_sync_feature_enabled) {
    BuildSyncInfoWithoutCallToAction(
        l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON),
        base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                            base::Unretained(this)));
    return;
  }

  // If there's no error and sync-the-feature is disabled, show a sync promo.
  // For a signed-in user, the promo just opens the "turn on sync" dialog.
  // For a web-only signed-in user in the UNO model, the promo signs the user on
  // Chrome and opens the "turn on sync" dialog.
  // For a signed-out user, it prompts for sign-in first.
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info_for_promos =
      signin_ui_util::GetSingleAccountForPromos(identity_manager);
  std::u16string description;
  std::u16string button_text;
  ActionableItem button_type = ActionableItem::kSigninAccountButton;
  bool show_account_card = false;
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kAvatarBubbleSignIn;

  signin_metrics::PromoAction promo_action =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

  if (!account_info.IsEmpty()) {
    if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info.account_id)) {
      // Sign-in pending state.
      button_type = ActionableItem::kSigninReauthButton;
      description =
          l10n_util::GetStringUTF16(IDS_SIGNIN_PAUSED_USER_MENU_VERIFY_MESSAGE);
      button_text =
          l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON);
    } else {
      // Signed-in not-syncing state.
      description = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO);
      button_text = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
    }
  } else if (!account_info_for_promos.IsEmpty()) {
    // Web-only signed-in state.
    account_info = account_info_for_promos;
    access_point =
        signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
    description =
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION);
    button_text = l10n_util::GetStringFUTF16(
        IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
        base::UTF8ToUTF16(!account_info_for_promos.given_name.empty()
                              ? account_info_for_promos.given_name
                              : account_info_for_promos.email));
    button_type = ActionableItem::kEnableSyncForWebOnlyAccountButton;
    show_account_card = true;
    promo_action = signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;
  } else {
    // Not signed in state.
    access_point =
        signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
    description =
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION);
    button_text =
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON);
    button_type = ActionableItem::kSigninButton;
    promo_action = signin_metrics::PromoAction::
        PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
  }
  signin_metrics::LogSignInOffered(access_point, promo_action);

  CHECK(!description.empty());
  CHECK(!button_text.empty());
  BuildSyncInfoWithCallToAction(
      description, button_text,
      base::BindRepeating(&ProfileMenuView::OnSigninButtonClicked,
                          base::Unretained(this), account_info, button_type,
                          access_point),
      /*show_sync_badge=*/false,
      show_account_card ? account_info_for_promos : AccountInfo());
}

ProfileMenuViewBase::IdentitySectionParams
ProfileMenuView::GetIdentitySectionParams(const ProfileAttributesEntry& entry) {
  CHECK(switches::IsImprovedSigninUIOnDesktopEnabled());

  Profile* profile = browser()->profile();
  const std::optional<AvatarSyncErrorType> error =
      GetAvatarSyncErrorType(profile);
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const bool is_sync_feature_enabled =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  const CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo primary_extended_account_info =
      identity_manager->FindExtendedAccountInfo(primary_account_info);
  CoreAccountInfo account_info_for_signin_action = primary_account_info;

  const bool is_dasherless_profile = entry.IsDasherlessManagement();

  IdentitySectionParams params;
  params.title = GetProfileIdentifier(entry);
  profiles::PlaceholderAvatarIconParams icon_params = {.has_padding = true,
                                                       .has_background = false};
  params.profile_image = ui::ImageModel::FromImage(
      primary_extended_account_info.account_image.IsEmpty()
          ? entry.GetAvatarIcon(kIdentityInfoImageSize,
                                /*use_high_res_file=*/true, icon_params)
          : primary_extended_account_info.account_image);

  ui::ImageModel* custom_management_image = nullptr;
  if (enterprise_util::CanShowEnterpriseBadgingForMenu(profile)) {
    if (profile->IsChild()) {
      params.header_string = l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    } else {
      params.header_string =
          l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PROFILE_MANAGED_HEADER);
      custom_management_image =
          policy::ManagementServiceFactory::GetForProfile(profile)
              ->GetManagementIconForProfile();
    }

    params.header_action =
        base::BindRepeating(&ProfileMenuView::OnProfileManagementButtonClicked,
                            base::Unretained(this));
    if (custom_management_image) {
      params.header_image = *custom_management_image;
    } else {
      params.header_image = ui::ImageModel::FromVectorIcon(
          GetManagedUiIcon(profile), ui::kColorIcon);
    }
  }

  // Clarify Dasherless profile with subtitle while not adding the button.
  if (is_dasherless_profile) {
    params.subtitle =
        l10n_util::GetStringUTF16(IDS_PROFILES_DASHER_FEATURE_DISABLED_TITLE);
    return params;
  }

  if (web_app::AppBrowserController::IsWebApp(browser())) {
    if (!primary_account_info.email.empty()) {
      params.subtitle = base::UTF8ToUTF16(primary_account_info.email);
    }
    return params;
  }

  // Sync error, including "paused".
  if (error.has_value()) {
    params.subtitle = GetAvatarSyncErrorDescription(
        *error, is_sync_feature_enabled, primary_account_info.email);
    params.button_text = GetSyncErrorButtonText(error.value());
    params.button_action =
        base::BindRepeating(&ProfileMenuView::OnSyncErrorButtonClicked,
                            base::Unretained(this), error.value());
    params.has_dotted_ring = true;
    return params;
  }

  ActionableItem button_type = ActionableItem::kSigninAccountButton;
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kAvatarBubbleSignIn;
  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignedOut:
      if (profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
        button_type = ActionableItem::kSigninButton;
        access_point =
            signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
        params.subtitle = l10n_util::GetStringUTF16(
            IDS_PROFILE_MENU_SIGNIN_PROMO_DESCRIPTION);
        params.button_text =
            l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON);
      }
      break;
    case signin_util::SignedInState::kWebOnlySignedIn: {
      access_point =
          signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
      AccountInfo account_info_for_promos =
          signin_ui_util::GetSingleAccountForPromos(identity_manager);
      account_info_for_signin_action = account_info_for_promos;
      params.subtitle = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PEOPLE_ACCOUNT_AWARE_SIGNIN_ACCOUNT_ROW_SUBTITLE_WITH_EMAIL,
          base::UTF8ToUTF16(account_info_for_promos.email));
      params.button_text = l10n_util::GetStringFUTF16(
          IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
          base::UTF8ToUTF16(!account_info_for_promos.given_name.empty()
                                ? account_info_for_promos.given_name
                                : account_info_for_promos.email));
      gfx::Image account_image;
      if (account_info_for_promos.account_image.IsEmpty()) {
        // No account image, use a placeholder.
        ProfileAttributesEntry* profile_attributes =
            g_browser_process->profile_manager()
                ->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile->GetPath());
        account_image = profile_attributes->GetAvatarIcon(
            /*size_for_placeholder_avatar=*/kIdentityImageSizeForButton,
            /*use_high_res_file=*/true,
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                browser()->window()->GetColorProvider()->GetColor(
                    ui::kColorButtonBackgroundProminent)));
      } else {
        account_image = account_info_for_promos.account_image;
      }
      params.button_image =
          ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
              account_image,
              /*width=*/kIdentityImageSizeForButton,
              /*height=*/kIdentityImageSizeForButton, profiles::SHAPE_CIRCLE));
      break;
    }
    case signin_util::SignedInState::kSignedIn:
      params.subtitle = l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO);
      params.button_text =
          l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
      break;
    case signin_util::SignedInState::kSyncing:
      // No button.
      params.subtitle = base::UTF8ToUTF16(primary_account_info.email);
      break;
    case signin_util::SignedInState::kSignInPending:
      button_type = ActionableItem::kSigninReauthButton;
      params.subtitle = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PENDING_STATE_DESCRIPTION,
          base::UTF8ToUTF16(primary_account_info.email));
      params.button_text =
          l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON);
      params.has_dotted_ring = true;
      break;
    case signin_util::SignedInState::kSyncPaused:
      // Sync paused is covered by the sync errors path.
      NOTREACHED();
  }

  if (!params.button_text.empty()) {
    params.button_action = base::BindRepeating(
        &ProfileMenuView::OnSigninButtonClicked, base::Unretained(this),
        account_info_for_signin_action, button_type, access_point);
  }

  return params;
}

void ProfileMenuView::BuildIdentityWithCallToAction() {
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    // May happen if the profile is being deleted. https://crbug.com/1040079
    return;
  }
  SetProfileIdentityWithCallToAction(GetIdentitySectionParams(*entry));
}

void ProfileMenuView::BuildAutofillSettingsButton() {
  CHECK(!browser()->profile()->IsGuestSession());
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_AUTOFILL_SETTINGS_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnAutofillSettingsButtonClicked,
                          base::Unretained(this)),
      vector_icons::kPasswordManagerIcon);
}

void ProfileMenuView::BuildCustomizeProfileButton() {
  CHECK(!browser()->profile()->IsGuestSession());
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_CUSTOMIZE_PROFILE_BUTTON),
      base::BindRepeating(&ProfileMenuView::OnEditProfileButtonClicked,
                          base::Unretained(this)),
      vector_icons::kEditChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildChromeAccountSettingsButton() {
  Profile* profile = browser()->profile();
  CHECK(!profile->IsGuestSession());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager) {
    return;
  }

  // Show the settings button when signed in to Chrome or to the web.
  bool should_show_settings_button =
      !identity_manager->GetExtendedAccountInfoForAccountsWithRefreshToken()
           .empty();

  int message_id = IDS_PROFILE_MENU_OPEN_ACCOUNT_SETTINGS;
  const gfx::VectorIcon* icon = &vector_icons::kSettingsChromeRefreshIcon;
  if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
    if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
      should_show_settings_button = true;
    }
    if (signin_util::GetSignedInState(identity_manager) ==
        signin_util::SignedInState::kSyncing) {
      // Indicates clearly that Sync is ON.
      message_id = IDS_PROFILES_OPEN_SYNC_SETTINGS_BUTTON;
      icon = &kSyncChromeRefreshIcon;
    }
  } else {
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      // Do not show if sync is enabled with the legacy design, because there is
      // already a similar button in the sync info card.
      should_show_settings_button = false;
    }
  }

  if (!should_show_settings_button) {
    return;
  }

  AddFeatureButton(
      l10n_util::GetStringUTF16(message_id),
      base::BindRepeating(&ProfileMenuView::OnSyncSettingsButtonClicked,
                          base::Unretained(this)),
      *icon);
}

void ProfileMenuView::MaybeBuildManageGoogleAccountButton() {
  Profile* profile = browser()->profile();
  CHECK(!profile->IsGuestSession());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager) {
    return;
  }

  bool show_button = true;
  switch (signin_util::GetSignedInState(identity_manager)) {
    case signin_util::SignedInState::kSignInPending:
      if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
        show_button = false;
      }
      break;
    case signin_util::SignedInState::kSyncPaused:
    case signin_util::SignedInState::kSignedOut:
    case signin_util::SignedInState::kWebOnlySignedIn:
      show_button = false;
      break;
    case signin_util::SignedInState::kSignedIn:
    case signin_util::SignedInState::kSyncing:
      // Show the button.
      break;
  }
  if (!show_button) {
    return;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The Google G icon needs to be shrunk, so it won't look too big compared
  // to the other icons.
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
      base::BindRepeating(&ProfileMenuView::OnManageGoogleAccountButtonClicked,
                          base::Unretained(this)),
      vector_icons::kGoogleGLogoIcon,
      /*icon_to_image_ratio=*/0.75f);
#else
  AddFeatureButton(
      l10n_util::GetStringUTF16(IDS_SETTINGS_MANAGE_GOOGLE_ACCOUNT),
      base::BindRepeating(&ProfileMenuView::OnManageGoogleAccountButtonClicked,
                          base::Unretained(this)));
#endif
}

void ProfileMenuView::MaybeBuildCloseBrowsersButton() {
  Profile* profile = browser()->profile();
  const int window_count = CountBrowsersFor(profile);
  base::RepeatingClosure callback = base::BindRepeating(
      &ProfileMenuView::OnExitProfileButtonClicked, base::Unretained(this));
  int button_title_id = IDS_PROFILE_MENU_CLOSE_PROFILE_X_WINDOWS_BUTTON;

  if (profile->IsGuestSession()) {
    button_title_id =
        base::FeatureList::IsEnabled(switches::kEnableImprovedGuestProfileMenu)
            ? IDS_GUEST_PROFILE_MENU_CLOSE_X_WINDOWS_BUTTON
            : IDS_GUEST_PROFILE_MENU_CLOSE_BUTTON;
  } else if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
    // Show the button only if the current profile has multiple windows open.
    if (window_count <= 1) {
      return;
    }

    // And there are multiple profiles open.
    std::vector<Profile*> loaded_profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    bool other_profile_open =
        std::any_of(loaded_profiles.begin(), loaded_profiles.end(),
                    [&profile](Profile* loaded_profile) {
                      if (loaded_profile == profile) {
                        return false;
                      }
                      return CountBrowsersFor(loaded_profile) > 0;
                    });
    if (!other_profile_open) {
      return;
    }
  }

  AddFeatureButton(
      l10n_util::GetPluralStringFUTF16(button_title_id, window_count),
      std::move(callback), vector_icons::kCloseChromeRefreshIcon);
}

void ProfileMenuView::MaybeBuildSignoutButton() {
  Profile* profile = browser()->profile();
  CHECK(!profile->IsGuestSession());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  const bool hide_signout_button_for_managed_profiles =
      enterprise_util::UserAcceptedAccountManagement(profile);

  const bool add_sign_out_button =
      HasUnconstentedProfile(profile) &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      !hide_signout_button_for_managed_profiles;
  if (!add_sign_out_button) {
    return;
  }

  std::u16string signout_button_text;
  // Note: Sign out button is only added if there is a signed profile with no
  // sync consent, so there is no need to check these conditions for the sign in
  // pending state.
  const bool signin_pending =
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  signout_button_text = l10n_util::GetStringUTF16(
      signin_pending ? IDS_PROFILE_MENU_SIGN_OUT_WHEN_SIGNIN_PENDING
                     : IDS_PROFILE_MENU_SIGN_OUT);
  AddFeatureButton(signout_button_text,
                   base::BindRepeating(&ProfileMenuView::OnSignoutButtonClicked,
                                       base::Unretained(this)),
                   switches::IsImprovedSigninUIOnDesktopEnabled()
                       ? kLogoutIcon
                       : kSignOutIcon);
}

void ProfileMenuView::BuildFeatureButtons() {
  CHECK(!browser()->profile()->IsGuestSession());
  if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
    BuildAutofillSettingsButton();
    MaybeBuildManageGoogleAccountButton();
    BuildCustomizeProfileButton();
    MaybeBuildChromeAccountSettingsButton();
    MaybeBuildCloseBrowsersButton();
    MaybeBuildSignoutButton();
    return;
  }

  BuildCustomizeProfileButton();
  MaybeBuildChromeAccountSettingsButton();
  MaybeBuildManageGoogleAccountButton();
  MaybeBuildCloseBrowsersButton();
  MaybeBuildSignoutButton();
}

void ProfileMenuView::GetProfilesForOtherProfilesSection(
    std::vector<ProfileAttributesEntry*>& available_profiles,
    bool& show_guest_in_other_profiles_section) const {
  CHECK(!browser()->profile()->IsGuestSession());
#if BUILDFLAG(IS_MAC)
  const bool is_regular_web_app =
      web_app::AppBrowserController::IsWebApp(browser()) &&
      (browser()->app_controller()->app_id() != ash::kPasswordManagerAppId);
  std::set<base::FilePath> available_profile_paths;
  if (is_regular_web_app) {
    available_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(
            browser()->app_controller()->app_id());
  }
#endif

  auto profile_entries = g_browser_process->profile_manager()
                             ->GetProfileAttributesStorage()
                             .GetAllProfilesAttributesSortedByNameWithCheck();
  for (ProfileAttributesEntry* profile_entry : profile_entries) {
    // The current profile is excluded.
    if (profile_entry->GetPath() == browser()->profile()->GetPath()) {
      continue;
    }
    if (profile_entry->IsOmitted()) {
      continue;
    }

#if BUILDFLAG(IS_MAC)
    if (is_regular_web_app &&
        !available_profile_paths.contains(profile_entry->GetPath())) {
      continue;
    }
#endif

    available_profiles.push_back(profile_entry);
  }

  show_guest_in_other_profiles_section =
      profiles::IsGuestModeEnabled(*browser()->profile()) &&
      !web_app::AppBrowserController::IsWebApp(browser()) &&
      !switches::IsImprovedSigninUIOnDesktopEnabled();
}

void ProfileMenuView::BuildOtherProfilesSection(
    const std::vector<ProfileAttributesEntry*>& available_profiles,
    bool show_guest_in_other_profiles_section) {
  for (ProfileAttributesEntry* profile_entry : available_profiles) {
    AddAvailableProfile(
        ui::ImageModel::FromImage(profile_entry->GetAvatarIcon(
            switches::IsImprovedSigninUIOnDesktopEnabled()
                ? kOtherProfileImageSize
                : kDeprecatedOtherProfileImageSize,
            /*use_high_res_file=*/true,
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                browser()->window()->GetColorProvider()->GetColor(
                    ui::kColorMenuBackground)))),
        profile_entry->GetName(),
        /*is_guest=*/false,
        base::BindRepeating(&ProfileMenuView::OnOtherProfileSelected,
                            base::Unretained(this), profile_entry->GetPath()));
  }

  if (show_guest_in_other_profiles_section) {
    AddAvailableProfile(
        profiles::GetGuestAvatar(),
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_OPEN_GUEST_PROFILE),
        /*is_guest=*/true,
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }
}

void ProfileMenuView::BuildProfileManagementFeatureButtons() {
  CHECK(!browser()->profile()->IsGuestSession());

  AddProfileManagementFeaturesSeparator();

  if (profiles::IsProfileCreationAllowed()) {
    AddProfileManagementFeatureButton(
        kAccountAddChromeRefreshIcon,
        l10n_util::GetStringUTF16(switches::IsImprovedSigninUIOnDesktopEnabled()
                                      ? IDS_PROFILE_MENU_ADD_PROFILE
                                      : IDS_PROFILE_MENU_ADD_NEW_PROFILE),
        base::BindRepeating(&ProfileMenuView::OnAddNewProfileButtonClicked,
                            base::Unretained(this)));
  }

  if (switches::IsImprovedSigninUIOnDesktopEnabled() &&
      profiles::IsGuestModeEnabled(*browser()->profile()) &&
      !web_app::AppBrowserController::IsWebApp(browser())) {
    AddProfileManagementFeatureButton(
        kAccountBoxIcon,
        l10n_util::GetStringUTF16(IDS_PROFILE_MENU_OPEN_GUEST_PROFILE),
        base::BindRepeating(&ProfileMenuView::OnGuestProfileButtonClicked,
                            base::Unretained(this)));
  }

  AddProfileManagementFeatureButton(
      kAccountManageChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_MANAGE_PROFILES),
      base::BindRepeating(&ProfileMenuView::OnManageProfilesButtonClicked,
                          base::Unretained(this)));
}
