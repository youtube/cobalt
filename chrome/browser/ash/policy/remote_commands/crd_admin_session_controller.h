// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_ADMIN_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_ADMIN_SESSION_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_controller.h"
#include "chrome/browser/ash/policy/remote_commands/start_crd_session_job_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/session_id.h"
#include "remoting/host/mojom/remote_support.mojom-forward.h"

namespace policy {

// Controller that owns the admin initiated CRD session (if any).
//
// Will keep the session alive and active as long as this class lives.
// Deleting this class object will forcefully interrupt the active CRD session.
class CrdAdminSessionController : private StartCrdSessionJobDelegate {
 public:
  // Proxy class to establish a connection with the Remoting service.
  // Overwritten in unittests to inject a test service.
  class RemotingServiceProxy {
   public:
    virtual ~RemotingServiceProxy() = default;

    using StartSessionCallback = base::OnceCallback<void(
        remoting::mojom::StartSupportSessionResponsePtr response)>;
    using SessionIdCallback =
        base::OnceCallback<void(absl::optional<remoting::SessionId>)>;

    // Starts a new remote support session. `callback` is
    // called with the result.
    virtual void StartSession(
        remoting::mojom::SupportSessionParamsPtr params,
        const remoting::ChromeOsEnterpriseParams& enterprise_params,
        StartSessionCallback callback) = 0;

    // Checks if session information for a reconnectable session is stored,
    // and invokes `callback` with the id of the reconnectable session (or
    // absl::nullopt if there is none).
    virtual void GetReconnectableSessionId(SessionIdCallback callback) = 0;

    // Starts a new remote support session, which will resume the reconnectable
    // session with the given `session_id`.
    virtual void ReconnectToSession(remoting::SessionId session_id,
                                    StartSessionCallback callback) = 0;
  };

  CrdAdminSessionController();
  explicit CrdAdminSessionController(
      std::unique_ptr<RemotingServiceProxy> remoting_service);
  CrdAdminSessionController(const CrdAdminSessionController&) = delete;
  CrdAdminSessionController& operator=(const CrdAdminSessionController&) =
      delete;
  ~CrdAdminSessionController() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Init(PrefService* local_state,
            base::OnceClosure done_callback = base::DoNothing());

  StartCrdSessionJobDelegate& GetDelegate();

  void ClickNotificationButtonForTesting();

 private:
  class CrdHostSession;

  // Checks if there is a reconnectable session, and if so this will reconnect
  // to it. A session is reconnectable when it was created with
  // `SessionParameters::allow_reconnections` set. `done_callback` is invoked
  // either when we conclude there is no reconnectable session, or when the
  // reconnectable session has been re-established.
  void TryToReconnect(base::OnceClosure done_callback);

  bool IsCurrentSessionCurtained() const;

  // `DeviceCommandStartCrdSessionJob::Delegate` implementation:
  bool HasActiveSession() const override;
  void TerminateSession() override;
  void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      AccessCodeCallback success_callback,
      ErrorCallback error_callback,
      SessionEndCallback session_finished_callback) override;

  std::unique_ptr<RemotingServiceProxy> remoting_service_;
  std::unique_ptr<CrdHostSession> active_session_;
  std::unique_ptr<RemoteActivityNotificationController>
      notification_controller_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_ADMIN_SESSION_CONTROLLER_H_
