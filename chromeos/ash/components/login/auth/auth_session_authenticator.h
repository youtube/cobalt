// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/safe_mode_delegate.h"
#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AuthFailure;

class PrefService;

namespace ash {

class AuthStatusConsumer;

// Authenticator that authenticates user against ChromeOS cryptohome using
// AuthSession API.
// Parallel authentication attempts are not possible, this is guarded by
// resetting all callbacks bound to weak pointers.
// Generic flow for all authentication attempts:
// * Initialize AuthSession (and learn if user exists)
//   * For existing users:
//     * Transform keys if necessary
//     * Authenticate AuthSession
//     * Mount directory
//     * (Safe mode) Check ownership
//   * For new users:
//     * Transform keys if necessary
//     * Add user credentials
//     * Authenticate session with same credentials
//     * Mount home directory (with empty create request)

// There are several points where flows are customized:
//   * Additional configuration of StartAuthSessionRequest
//   * Additional configuration of MountRequest
//   * Customized error handling
//     (e.g. different "default" error for different user types)
//   * Different ways to hash plain text key
//   * Different ways to create crytohome key from key in UserContext

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    AuthSessionAuthenticator : public Authenticator {
 public:
  // `consumer` would receive notifications upon authentication success/failure/
  // triggering edge case.
  // `safe_mode_delegate` is an interface for detecting safe mode / checking if
  // user is indeed an owner.
  // `user_recorder` is used during user creation of the user when
  // challenge-response authentication is used, so that
  // CryptohomeKeyDelegateServiceProvider would work correctly.
  // `is_ephemeral_mount_enforced` forces usage of ephemeral mounts for regular
  // users. Should not be used for kiosks users.
  // `local_state` used to persist login-related state across reboots via
  // `UserDirectoryIntegrityManager`
  AuthSessionAuthenticator(
      AuthStatusConsumer* consumer,
      std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
      base::RepeatingCallback<void(const AccountId&)> user_recorder,
      bool is_ephemeral_mount_enforced,
      PrefService* local_state);

  // Authenticator overrides.
  void CompleteLogin(std::unique_ptr<UserContext> user_context) override;

  void AuthenticateToLogin(std::unique_ptr<UserContext> user_context) override;
  void AuthenticateToUnlock(std::unique_ptr<UserContext> user_context) override;
  void LoginOffTheRecord() override;
  void LoginAsPublicSession(const UserContext& user_context) override;
  void LoginAsKioskAccount(const AccountId& app_account_id,
                           bool ephemeral) override;
  void LoginAsArcKioskAccount(const AccountId& app_account_id,
                              bool ephemeral) override;
  void LoginAsWebKioskAccount(const AccountId& app_account_id,
                              bool ephemeral) override;
  void LoginAuthenticated(std::unique_ptr<UserContext> user_context) override;
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& error) override;
  void RecoverEncryptedData(std::unique_ptr<UserContext> user_context,
                            const std::string& old_password) override;
  void ResyncEncryptedData(std::unique_ptr<UserContext> user_context) override;

 protected:
  ~AuthSessionAuthenticator() override;

 private:
  using StartAuthSessionCallback =
      base::OnceCallback<void(bool user_exists,
                              std::unique_ptr<UserContext> context,
                              absl::optional<AuthenticationError> error)>;

  // Callbacks that handles auth session started for particular login flows.
  // |user_exists| indicates if cryptohome actually exists on the disk,
  // |context| at this point would contain authsession and meta info describing
  // the keys.
  void DoLoginAsPublicSession(bool user_exists,
                              std::unique_ptr<UserContext> context,
                              absl::optional<AuthenticationError> error);
  void DoLoginAsKiosk(bool ephemeral,
                      bool user_exists,
                      std::unique_ptr<UserContext> context,
                      absl::optional<AuthenticationError> error);
  void DoLoginAsExistingUser(bool user_exists,
                             std::unique_ptr<UserContext> context,
                             absl::optional<AuthenticationError> error);
  void DoCompleteLogin(bool user_exists,
                       std::unique_ptr<UserContext> context,
                       absl::optional<AuthenticationError> error);
  void DoUnlock(bool user_exists,
                std::unique_ptr<UserContext> context,
                absl::optional<AuthenticationError> error);

  // Common part of login logic shared by user creation flow and flow when
  // user have changed password elsewhere and decides to re-create cryptohome.
  void CompleteLoginImpl(std::unique_ptr<UserContext> user_context);
  void LoginAsKioskImpl(const AccountId& app_account_id,
                        user_manager::UserType user_type,
                        bool force_dircrypto,
                        bool ephemeral);

  // Helpers for starting the auth session and cleaning stale data during that.
  void StartAuthSessionForLogin(std::unique_ptr<UserContext> context,
                                bool ephemeral,
                                AuthSessionIntent intent,
                                StartAuthSessionCallback callback);
  void OnStartAuthSessionForLogin(std::unique_ptr<UserContext> original_context,
                                  bool ephemeral,
                                  AuthSessionIntent intent,
                                  StartAuthSessionCallback callback,
                                  bool user_exists,
                                  std::unique_ptr<UserContext> context,
                                  absl::optional<AuthenticationError> error);
  void RemoveStaleUserForEphemeral(
      const std::string& auth_session_id,
      std::unique_ptr<UserContext> original_context,
      AuthSessionIntent intent,
      StartAuthSessionCallback callback);
  void OnRemoveStaleUserForEphemeral(
      std::unique_ptr<UserContext> original_context,
      AuthSessionIntent intent,
      StartAuthSessionCallback callback,
      absl::optional<user_data_auth::RemoveReply> reply);
  void OnStartAuthSessionForLoginAfterStaleRemoval(
      StartAuthSessionCallback callback,
      bool user_exists,
      std::unique_ptr<UserContext> context,
      absl::optional<AuthenticationError> error);
  // Similar to `StartAuthSessionForLogin()`, but doesn't trigger the stale data
  // removal logic.
  void StartAuthSessionForLoggedIn(std::unique_ptr<UserContext> context,
                                   bool ephemeral,
                                   AuthSessionIntent intent,
                                   StartAuthSessionCallback callback);

  // Notifies `UserDirectoryIntegrityManager` that a user creation
  // process has started.
  void RecordCreatingNewUser(std::unique_ptr<UserContext> context,
                             AuthOperationCallback callback);

  // Notifies `UserDirectoryIntegrityManager` that the newly created user
  // has added a first auth factor.
  void RecordFirstAuthFactorAdded(std::unique_ptr<UserContext> context,
                                  AuthOperationCallback callback);

  void PrepareForNewAttempt(const std::string& method_id,
                            const std::string& long_desc);

  // Simple callback that notifies about mount success / failure.
  void NotifyAuthSuccess(std::unique_ptr<UserContext> context);
  void NotifyGuestSuccess(std::unique_ptr<UserContext> context);
  void NotifyFailure(AuthFailure::FailureReason reason,
                     std::unique_ptr<UserContext> context);

  // Handles errors specific to authenticating existing users with the password:
  //   if password is known to be correct (e.g. it comes from online auth flow),
  //   special error code would be raised in case of "incorrect password" to
  //   indicate a need to replace password.
  // Other errors are handled by `fallback`.
  void HandlePasswordChangeDetected(AuthErrorCallback fallback,
                                    std::unique_ptr<UserContext> context,
                                    AuthenticationError error);
  // Handles errors specific to the encryption migration:
  //   if the encryption migration is required, triggers OnOldEncryptionDetected
  //   method on the consumer.
  // Other errors are handled by `fallback`.
  void HandleMigrationRequired(AuthErrorCallback fallback,
                               std::unique_ptr<UserContext> context,
                               AuthenticationError error);

  bool ResolveCryptohomeError(AuthFailure::FailureReason default_error,
                              AuthenticationError& error);
  // Generic error handler, can be used as ErrorHandlingCallback when first
  // parameter is bound to a user type-specific failure reason.
  void ProcessCryptohomeError(AuthFailure::FailureReason default_reason,
                              std::unique_ptr<UserContext> user_context,
                              AuthenticationError error);
  // Check used for existing regular users - in safe mode would check
  // if home directory contains valid owner key. If key is not found,
  // would unmount directory and notify failure.
  void CheckOwnershipOperation(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);
  void OnSafeModeOwnershipCheck(std::unique_ptr<UserContext> context,
                                AuthOperationCallback callback,
                                bool is_owner);
  void OnUnmountForNonOwner(std::unique_ptr<UserContext> context,
                            absl::optional<AuthenticationError> error);

  // Save information about user so that it can be used by
  // `CryptohomeKeyDelegateServiceProvider`.
  void SaveKnownUser(std::unique_ptr<UserContext> context,
                     AuthOperationCallback callback);

  // TODO(b/275689019): remove this field and instead pass `ephemeral_mount` as
  // `Login*` functions parameter.
  //
  // Don't use this field to determine ephemeral mount for kiosk users.
  const bool is_ephemeral_mount_enforced_;
  base::RepeatingCallback<void(const AccountId&)> user_recorder_;
  std::unique_ptr<SafeModeDelegate> safe_mode_delegate_;
  std::unique_ptr<AuthFactorEditor> auth_factor_editor_;
  std::unique_ptr<AuthPerformer> auth_performer_;
  std::unique_ptr<HibernateManager> hibernate_manager_;
  std::unique_ptr<MountPerformer> mount_performer_;

  const base::raw_ptr<PrefService, DanglingUntriaged> local_state_;

  base::WeakPtrFactory<AuthSessionAuthenticator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
