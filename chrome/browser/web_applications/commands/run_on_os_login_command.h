// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;

enum class RunOnOsLoginAction {
  kSetModeInDBAndOS = 0,
  kSyncModeFromDBToOS = 1,
  kMaxValue = kSyncModeFromDBToOS
};

enum class RunOnOsLoginCommandCompletionState {
  kSuccessfulCompletion = 0,
  kCommandSystemShutDown = 1,
  kNotAllowedByPolicy = 2,
  kRunOnOsLoginModeAlreadyMatched = 3,
  kAppNotLocallyInstalled = 4,
  kOSHooksNotProperlySet = 5,
  kMaxValue = kOSHooksNotProperlySet
};

// This command persists run on os login data to the web_app DB
// and/or syncs the run on os login data with the OS integration hooks
// asynchronously.
class RunOnOsLoginCommand : public WebAppCommandTemplate<AppLock> {
 public:
  static std::unique_ptr<RunOnOsLoginCommand> CreateForSetLoginMode(
      const AppId& app_id,
      RunOnOsLoginMode login_mode,
      base::OnceClosure callback);
  static std::unique_ptr<RunOnOsLoginCommand> CreateForSyncLoginMode(
      const AppId& app_id,
      base::OnceClosure callback);
  ~RunOnOsLoginCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  void OnSyncSourceRemoved() override {}
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  RunOnOsLoginCommand(AppId app_id,
                      absl::optional<RunOnOsLoginMode> login_mode,
                      RunOnOsLoginAction set_or_sync_mode,
                      base::OnceClosure callback_);
  void Abort(RunOnOsLoginCommandCompletionState aborted_state);
  // Updates the Run On OS login state for the given app. If necessary, it also
  // updates the registration with the OS. This will take into account the
  // enterprise policy for the given app.
  void SetRunOnOsLoginMode();
  // Reads the expected Run On OS login state for a given app from the DB and
  // deploys that to the OS.
  // Note that this tries to avoid extra work by no-oping if the current
  // OS state matches what is calculated to be the desired stated.
  void SyncRunOnOsLoginMode();
  void UpdateRunOnOsLoginModeWithOsIntegration(
      base::RepeatingCallback<void(OsHooksErrors)> os_hooks_callback);
  void OnOsHooksSet(OsHooksErrors errors);
  void RecordCompletionState(
      RunOnOsLoginCommandCompletionState completion_state);

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  AppId app_id_;
  absl::optional<RunOnOsLoginMode> login_mode_;
  RunOnOsLoginAction set_or_sync_mode_;
  std::string stop_reason_;
  base::OnceClosure callback_ = base::DoNothing();
  bool completion_state_set_ = false;

  base::WeakPtrFactory<RunOnOsLoginCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_RUN_ON_OS_LOGIN_COMMAND_H_
