// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/login_performer.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

LoginPerformer::LoginPerformer(Delegate* delegate,
                               AuthEventsRecorder* metrics_recorder)
    : delegate_(delegate),
      auth_events_recorder_(metrics_recorder),
      last_login_failure_(AuthFailure(AuthFailure::NONE)) {
  DCHECK(auth_events_recorder_);
}

LoginPerformer::~LoginPerformer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Deleting LoginPerformer";
  if (authenticator_.get())
    authenticator_->SetConsumer(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, AuthStatusConsumer implementation:

void LoginPerformer::OnAuthFailure(const AuthFailure& failure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auth_events_recorder_->OnAuthFailure(failure.reason());

  LOG(ERROR) << "Login failure, reason=" << failure.reason()
             << ", error.state=" << failure.error().state();

  last_login_failure_ = failure;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAuthFailure,
                                weak_factory_.GetWeakPtr(), failure));
}

void LoginPerformer::OnAuthSuccess(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
      user_context.GetAccountId());
  bool is_login_offline =
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE;
  const bool is_ephemeral =
      user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          user_context.GetAccountId());
  SuccessReason reason = SuccessReason::OFFLINE_AND_ONLINE;
  if (!is_known_user || is_ephemeral) {
    reason = SuccessReason::ONLINE_ONLY;
  } else if (is_login_offline) {
    reason = SuccessReason::OFFLINE_ONLY;
  }

  auth_events_recorder_->OnLoginSuccess(reason,
                                        /*is_new_user=*/!is_known_user,
                                        is_login_offline, is_ephemeral);
  VLOG(1) << "LoginSuccess hash: " << user_context.GetUserIDHash();

  if (user_context.GetUserType() == user_manager::USER_TYPE_REGULAR ||
      user_context.GetUserType() == user_manager::USER_TYPE_CHILD) {
    LoadAndApplyEarlyPrefs(std::make_unique<UserContext>(user_context),
                           base::BindOnce(&LoginPerformer::OnEarlyPrefsApplied,
                                          weak_factory_.GetWeakPtr()));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAuthSuccess,
                                weak_factory_.GetWeakPtr(), user_context));
}

void LoginPerformer::OnEarlyPrefsApplied(
    std::unique_ptr<UserContext> context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Could not apply policies due to error:"
               << error->ToDebugString();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAuthSuccess,
                                weak_factory_.GetWeakPtr(), *context.get()));
}

void LoginPerformer::OnOffTheRecordAuthSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_events_recorder_->OnGuestLoginSuccess();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyOffTheRecordAuthSuccess,
                                weak_factory_.GetWeakPtr()));
}

void LoginPerformer::OnPasswordChangeDetectedLegacy(
    const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_events_recorder_->OnPasswordChange();
  password_changed_ = true;
  password_changed_callback_count_++;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoginPerformer::NotifyPasswordChangeDetectedLegacy,
                     weak_factory_.GetWeakPtr(), user_context));
}

void LoginPerformer::OnPasswordChangeDetected(
    std::unique_ptr<UserContext> user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_events_recorder_->OnPasswordChange();
  password_changed_ = true;
  DCHECK(user_context);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoginPerformer::NotifyPasswordChangeDetected,
                     weak_factory_.GetWeakPtr(), std::move(user_context)));
}

void LoginPerformer::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoginPerformer::NotifyOldEncryptionDetected,
                     weak_factory_.GetWeakPtr(), std::move(user_context),
                     has_incomplete_migration));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::PerformLogin(const UserContext& user_context,
                                  AuthorizationMode auth_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_mode_ = auth_mode;
  user_context_ = user_context;

  if (RunTrustedCheck(base::BindOnce(&LoginPerformer::DoPerformLogin,
                                     weak_factory_.GetWeakPtr(), user_context_,
                                     auth_mode))) {
    return;
  }
  DoPerformLogin(user_context_, auth_mode);
}

void LoginPerformer::DoPerformLogin(const UserContext& user_context,
                                    AuthorizationMode auth_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool wildcard_match = false;

  const AccountId& account_id = user_context.GetAccountId();
  if (!IsUserAllowlisted(account_id, &wildcard_match,
                         user_context.GetUserType())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAllowlistCheckFailure,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  switch (auth_mode_) {
    case AuthorizationMode::kExternal: {
      RunOnlineAllowlistCheck(
          account_id, wildcard_match, user_context.GetRefreshToken(),
          base::BindOnce(&LoginPerformer::StartLoginCompletion,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&LoginPerformer::NotifyAllowlistCheckFailure,
                         weak_factory_.GetWeakPtr()));
      break;
    }
    case AuthorizationMode::kInternal:
      StartAuthentication();
      break;
  }
}

void LoginPerformer::LoginAsPublicSession(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckPolicyForUser(user_context.GetAccountId())) {
    DCHECK(delegate_);
    delegate_->PolicyLoadFailed();
    return;
  }

  EnsureAuthenticator();
  authenticator_->LoginAsPublicSession(user_context);
}

void LoginPerformer::LoginOffTheRecord() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginOffTheRecord();
}

void LoginPerformer::LoginAsKioskAccount(const AccountId& app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsKioskAccount(
      app_account_id,
      user_manager::UserManager::Get()->IsEphemeralAccountId(app_account_id));
}

void LoginPerformer::LoginAsArcKioskAccount(
    const AccountId& arc_app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsArcKioskAccount(
      arc_app_account_id,
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          arc_app_account_id));
}

void LoginPerformer::LoginAsWebKioskAccount(
    const AccountId& web_app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsWebKioskAccount(
      web_app_account_id,
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          web_app_account_id));
}

void LoginPerformer::LoginAuthenticated(
    std::unique_ptr<UserContext> user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAuthenticated(std::move(user_context));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->RecoverEncryptedData(
      std::make_unique<UserContext>(user_context_), old_password);
  user_context_.ClearSecrets();
}

void LoginPerformer::ResyncEncryptedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->ResyncEncryptedData(
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          user_context_.GetAccountId()),
      std::make_unique<UserContext>(user_context_));
  user_context_.ClearSecrets();
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::NotifyAllowlistCheckFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->AllowlistCheckFailed(user_context_.GetAccountId().GetUserEmail());
}

void LoginPerformer::NotifyAuthFailure(const AuthFailure& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnAuthFailure(error);
}

void LoginPerformer::NotifyAuthSuccess(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  // After delegate_->OnAuthSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  delegate_->OnAuthSuccess(user_context);
}

void LoginPerformer::NotifyOffTheRecordAuthSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnOffTheRecordAuthSuccess();
}

void LoginPerformer::NotifyPasswordChangeDetectedLegacy(
    const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  user_context_ = user_context;
  delegate_->OnPasswordChangeDetectedLegacy(user_context);
}

void LoginPerformer::NotifyPasswordChangeDetected(
    std::unique_ptr<UserContext> user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(user_context);
  delegate_->OnPasswordChangeDetected(std::move(user_context));
}

void LoginPerformer::NotifyOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnOldEncryptionDetected(std::move(user_context),
                                     has_incomplete_migration);
}

void LoginPerformer::StartLoginCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Online login completion started.";
  LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  EnsureAuthenticator();
  authenticator_->CompleteLogin(
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          user_context_.GetAccountId()),
      std::make_unique<UserContext>(user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::StartAuthentication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Offline auth started.";
  LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  DCHECK(delegate_);
  EnsureAuthenticator();
  authenticator_->AuthenticateToLogin(
      user_manager::UserManager::Get()->IsEphemeralAccountId(
          user_context_.GetAccountId()),
      std::make_unique<UserContext>(user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::EnsureAuthenticator() {
  authenticator_ = CreateAuthenticator();
}
}  // namespace ash
