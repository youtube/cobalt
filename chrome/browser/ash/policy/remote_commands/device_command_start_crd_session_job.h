// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/start_crd_session_job_delegate.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Remote command that would start Chrome Remote Desktop host and return auth
// code. This command is usable only for devices running Kiosk sessions, for
// Affiliated Users and for Managed Guest Sessions.
class DeviceCommandStartCrdSessionJob : public RemoteCommandJob {
 public:
  using OAuthTokenCallback = base::OnceCallback<void(const std::string&)>;
  using Delegate = StartCrdSessionJobDelegate;

  explicit DeviceCommandStartCrdSessionJob(Delegate& delegate);
  ~DeviceCommandStartCrdSessionJob() override;

  DeviceCommandStartCrdSessionJob(const DeviceCommandStartCrdSessionJob&) =
      delete;
  DeviceCommandStartCrdSessionJob& operator=(
      const DeviceCommandStartCrdSessionJob&) = delete;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

  // Set a Fake OAuth token that will be used once the next time we need to
  // fetch an oauth token.
  void SetOAuthTokenForTest(const std::string& token);

  // This enum can't be renumbered because it is logged to UMA.
  // TODO(b/261425261): Remove this enum when the Uma histogram is removed.
  enum class UmaSessionType {
    kAutoLaunchedKiosk = 0,
    kAffiliatedUser = 1,
    kManagedGuestSession = 2,
    kManuallyLaunchedKiosk = 3,
    kMaxValue = kManuallyLaunchedKiosk
  };

 protected:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;
  void TerminateImpl() override;

 private:
  class OAuthTokenFetcher;

  void CheckManagedNetworkASync(base::OnceClosure on_success);
  void FetchOAuthTokenASync(OAuthTokenCallback on_success);
  void StartCrdHostAndGetCode(const std::string& token);
  void FinishWithSuccess(const std::string& access_code);
  // Finishes command with error code and optional message.
  void FinishWithError(ResultCode result_code, const std::string& message);
  void FinishWithNotIdleError();

  bool UserTypeSupportsCrd() const;
  UmaSessionType GetUmaSessionType() const;
  CrdSessionType GetCrdSessionType() const;
  bool IsDeviceIdle() const;

  std::string GetRobotAccountUserName() const;
  bool ShouldShowConfirmationDialog() const;
  bool ShouldTerminateUponInput() const;
  bool ShouldAllowReconnections() const;
  bool ShouldAllowTroubleshootingTools() const;
  bool ShouldShowTroubleshootingTools() const;
  bool ShouldAllowFileTransfer() const;

  Delegate::ErrorCallback GetErrorCallback();

  std::unique_ptr<OAuthTokenFetcher> oauth_token_fetcher_;

  // The callback that will be called when the access code was successfully
  // obtained or when this command failed.
  CallbackWithResult result_callback_;

  // -- Command parameters --

  // Defines whether connection attempt to active user should succeed or fail.
  base::TimeDelta idleness_cutoff_;

  // True if the admin has confirmed that they want to start the CRD session,
  // while a user is currently using the device.
  bool acked_user_presence_ = false;

  // True if the admin requested a curtained remote access session.
  bool curtain_local_user_session_ = false;

  // The email address of the admin user who issued the remote command.
  absl::optional<std::string> admin_email_;

  // -- End of command parameters --

  // Fake OAuth token that will be used once the next time we need to fetch an
  // oauth token.
  absl::optional<std::string> oauth_token_for_test_;

  // The Delegate is used to interact with chrome services and CRD host.
  const raw_ref<Delegate> delegate_;

  base::WeakPtrFactory<DeviceCommandStartCrdSessionJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_START_CRD_SESSION_JOB_H_
