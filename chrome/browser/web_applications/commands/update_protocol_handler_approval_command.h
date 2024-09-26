// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_PROTOCOL_HANDLER_APPROVAL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_PROTOCOL_HANDLER_APPROVAL_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
enum class ApiApprovalState;

// Updates the approved or disallowed protocol list for the given app. If
// necessary, it also updates the protocol registration with the OS.
class UpdateProtocolHandlerApprovalCommand
    : public WebAppCommandTemplate<AppLock> {
 public:
  UpdateProtocolHandlerApprovalCommand(const AppId& app_id,
                                       const std::string& protocol_scheme,
                                       ApiApprovalState approval_state,
                                       base::OnceClosure callback);

  ~UpdateProtocolHandlerApprovalCommand() override;

  // WebAppCommandTemplate<AppLock>:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

 private:
  void OnProtocolHandlersSynchronizeComplete(
      const std::vector<custom_handlers::ProtocolHandler>&
          original_protocol_handlers,
      OsIntegrationManager& os_integration_manager);
  void OnProtocolHandlersUpdated();

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  const AppId app_id_;
  std::string protocol_scheme_;
  const ApiApprovalState approval_state_;
  base::OnceClosure callback_;

  base::Value::Dict debug_info_;

  base::WeakPtrFactory<UpdateProtocolHandlerApprovalCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_PROTOCOL_HANDLER_APPROVAL_COMMAND_H_
