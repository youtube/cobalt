// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"

#include <optional>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"

namespace {

std::optional<ProfileMetrics::ProfileSignedInFlowOutcome> GetSyncOutcome(
    bool enterprise_account,
    bool sync_disabled,
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // The decision of the user is not relevant for the metric.
  if (sync_disabled) {
    return ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSyncDisabled;
  }

  switch (result) {
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      return enterprise_account
                 ? ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSync
                 : ProfileMetrics::ProfileSignedInFlowOutcome::kConsumerSync;
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      return enterprise_account ? ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kEnterpriseSyncSettings
                                : ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kConsumerSyncSettings;
    case LoginUIService::ABORT_SYNC:
      return enterprise_account ? ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kEnterpriseSigninOnly
                                : ProfileMetrics::ProfileSignedInFlowOutcome::
                                      kConsumerSigninOnly;
    case LoginUIService::UI_CLOSED:
      // The metric is recorded elsewhere.
      return std::nullopt;
  }
}

void OpenSettingsInBrowser(Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    base::debug::DumpWithoutCrashing();
    return;
  }
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}

}  // namespace

ProfilePickerTurnSyncOnDelegate::ProfilePickerTurnSyncOnDelegate(
    base::WeakPtr<ProfilePickerSignedInFlowController> controller,
    Profile* profile)
    : controller_(controller), profile_(profile) {}

ProfilePickerTurnSyncOnDelegate::~ProfilePickerTurnSyncOnDelegate() = default;

void ProfilePickerTurnSyncOnDelegate::ShowLoginError(
    const SigninUIError& error) {
  LogOutcome(ProfileMetrics::ProfileSignedInFlowOutcome::kLoginError);

  // If the controller is null we cannot treat the error.
  if (!controller_) {
    return;
  }

  // Show the profile switch confirmation screen inside of the profile picker if
  // the user cannot sign in because the account already used by another
  // profile.
  if (error.type() ==
      SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile) {
    controller_->SwitchToProfileSwitch(error.another_profile_path());
    return;
  }

  // Abort the flow completely and reset the host in case of ForceSignin if the
  // user is not allowed to sign in by policy with this account. In
  // non-ForceSignin, the user can still browse and be signed in but cannot
  // enable sync.
  if (signin_util::IsForceSigninEnabled() &&
      error.type() ==
          SigninUIError::Type::kUsernameNotAllowedByPatternFromPrefs) {
    controller_->ResetHostAndShowErrorDialog(
        ForceSigninUIError::SigninPatternNotMatching(
            base::UTF16ToUTF8(error.email())));
    return;
  }

  // Open the browser and when it's done, show the login error.
  controller_->FinishAndOpenBrowser(PostHostClearedCallback(base::BindOnce(
      &TurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser, error)));
}

void ProfilePickerTurnSyncOnDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    signin::SigninChoiceCallback callback) {
  // A brand new profile cannot have a conflict in sync accounts.
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback) {
  enterprise_account_ = true;
  // In this flow, the enterprise confirmation is replaced by an enterprise
  // notice screen. Knowing if sync is enabled is needed for the screen. Thus,
  // it is delayed until either ShowSyncConfirmation() or
  // ShowSyncDisabledConfirmation() gets called.
  // Assume an implicit "Continue" here.
  std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  return;
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_confirmation_callback_ = std::move(callback);

  if (enterprise_account_) {
    // First show the notice screen and only after that (if the user proceeds
    // with the flow) the sync consent.
    ShowManagedUserNotice(
        ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled);
    return;
  }

  ShowSyncConfirmationScreen();
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  DCHECK(callback);
  sync_disabled_ = true;

  sync_confirmation_callback_ = std::move(callback);
  ShowManagedUserNotice(is_managed_account
                            ? ManagedUserProfileNoticeUI::ScreenType::
                                  kEntepriseAccountSyncDisabled
                            : ManagedUserProfileNoticeUI::ScreenType::
                                  kConsumerAccountSyncDisabled);
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncSettings() {
  // Open the browser and when it's done, open settings in the browser.
  if (controller_) {
    controller_->FinishAndOpenBrowser(
        PostHostClearedCallback(base::BindOnce(&OpenSettingsInBrowser)));
  }
}

void ProfilePickerTurnSyncOnDelegate::SwitchToProfile(Profile* new_profile) {
  // A brand new profile cannot have preexisting syncable data and thus
  // switching to another profile does never get offered.
  NOTREACHED();
}

void ProfilePickerTurnSyncOnDelegate::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  // No need to listen to further confirmations any more.
  DCHECK(scoped_login_ui_service_observation_.IsObservingSource(
      LoginUIServiceFactory::GetForProfile(profile_)));
  scoped_login_ui_service_observation_.Reset();

  // If the user declines enabling sync while browser sign-in is forced, prevent
  // them from going further by cancelling the creation of this profile.
  // It does not apply to managed accounts.
  // TODO(crbug.com/40280466): Align Managed and Consumer accounts.
  if (signin_util::IsForceSigninEnabled() &&
      !enterprise_util::ProfileCanBeManaged(profile_) &&
      result == LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC) {
    HandleCancelSigninChoice(
        ProfileMetrics::ProfileSignedInFlowOutcome::kForceSigninSyncNotGranted);
    return;
  }

  std::optional<ProfileMetrics::ProfileSignedInFlowOutcome> outcome =
      GetSyncOutcome(enterprise_account_, sync_disabled_, result);
  if (outcome) {
    LogOutcome(*outcome);
  }

  FinishSyncConfirmation(result);
}

void ProfilePickerTurnSyncOnDelegate::ShowSyncConfirmationScreen() {
  DCHECK(sync_confirmation_callback_);
  DCHECK(!scoped_login_ui_service_observation_.IsObserving());
  scoped_login_ui_service_observation_.Observe(
      LoginUIServiceFactory::GetForProfile(profile_));

  if (controller_) {
    controller_->SwitchToSyncConfirmation();
  }
}

void ProfilePickerTurnSyncOnDelegate::FinishSyncConfirmation(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  DCHECK(sync_confirmation_callback_);
  std::move(sync_confirmation_callback_).Run(result);
}

void ProfilePickerTurnSyncOnDelegate::ShowManagedUserNotice(
    ManagedUserProfileNoticeUI::ScreenType type) {
  DCHECK(sync_confirmation_callback_);
  // Unretained as the delegate lives until `sync_confirmation_callback_` gets
  // called and thus always outlives the notice screen.
  if (controller_) {
    controller_->SwitchToManagedUserProfileNotice(
        type, base::BindOnce(
                  &ProfilePickerTurnSyncOnDelegate::OnManagedUserNoticeClosed,
                  base::Unretained(this), type));
  }
}

void ProfilePickerTurnSyncOnDelegate::HandleCancelSigninChoice(
    ProfileMetrics::ProfileSignedInFlowOutcome outcome) {
  LogOutcome(outcome);
  // The callback provided by TurnSyncOnHelper must be called, UI_CLOSED
  // makes sure the final callback does not get called. It does not matter
  // what happens to sync as the signed-in profile creation gets cancelled
  // right after.
  FinishSyncConfirmation(LoginUIService::UI_CLOSED);
  ProfilePicker::CancelSignedInFlow();
}

void ProfilePickerTurnSyncOnDelegate::OnManagedUserNoticeClosed(
    ManagedUserProfileNoticeUI::ScreenType type,
    signin::SigninChoice choice) {
  if (choice == signin::SIGNIN_CHOICE_CANCEL) {
    // Enforce that the account declined the enterprise management. This value
    // could have been set as a result of
    // `ProfilePickerTurnSyncOnDelegate::ShowEnterpriseAccountConfirmation()`
    // continuing by default prior in the flow.
    signin::ClearProfileWithManagedAccounts(profile_);

    HandleCancelSigninChoice(ProfileMetrics::ProfileSignedInFlowOutcome::
                                 kAbortedOnEnterpriseWelcome);
    return;
  }

  // For the Profile Picker flows, the profile should always be new. Other flows
  // also handle whether data from the existing profile should be merged.
  DCHECK_EQ(choice, signin::SIGNIN_CHOICE_NEW_PROFILE);

  switch (type) {
    case ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled:
      ShowSyncConfirmationScreen();
      return;
    case ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncDisabled:
    case ManagedUserProfileNoticeUI::ScreenType::kConsumerAccountSyncDisabled:
      // Logging kEnterpriseSyncDisabled for consumer accounts on managed
      // devices is a pre-existing minor imprecision in reporting of this metric
      // that's not worth fixing.
      LogOutcome(
          ProfileMetrics::ProfileSignedInFlowOutcome::kEnterpriseSyncDisabled);
      // SYNC_WITH_DEFAULT_SETTINGS encodes that the user wants to continue
      // (despite sync being disabled).
      // TODO (crbug.com/1141341): Split the enum for sync disabled / rename the
      // entries to better match the situation.
      FinishSyncConfirmation(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
      break;
    case ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC:
    case ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation:
      NOTREACHED() << "The profile picker should not show a managed user "
                      "notice that prompts for profile creation";
  }
}

void ProfilePickerTurnSyncOnDelegate::LogOutcome(
    ProfileMetrics::ProfileSignedInFlowOutcome outcome) {
  ProfileMetrics::LogProfileAddSignInFlowOutcome(outcome);
}
