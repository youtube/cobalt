// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace webapps {
enum class InstallResultCode;
}  // namespace webapps

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;

// After all app windows have closed, this command runs to perform the last few
// steps of writing the data to the DB.
class ManifestUpdateFinalizeCommand : public WebAppCommandTemplate<AppLock> {
 public:
  using ManifestWriteCallback = base::OnceCallback<
      void(const GURL& url, const AppId& app_id, ManifestUpdateResult result)>;

  ManifestUpdateFinalizeCommand(
      const GURL& url,
      const AppId& app_id,
      WebAppInstallInfo install_info,
      ManifestWriteCallback write_callback,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive);

  ~ManifestUpdateFinalizeCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  void OnSyncSourceRemoved() override {}
  void OnShutdown() override;
  base::Value ToDebugValue() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

  base::WeakPtr<ManifestUpdateFinalizeCommand> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnInstallationComplete(const AppId& app_id,
                              webapps::InstallResultCode code,
                              OsHooksErrors os_hooks_errors);
  void CompleteCommand(webapps::InstallResultCode code,
                       ManifestUpdateResult result);

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  const GURL url_;
  const AppId app_id_;
  WebAppInstallInfo install_info_;
  ManifestWriteCallback write_callback_;
  // Two KeepAlive objects, to make sure that manifest update writes
  // still happen even though the app window has closed.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  base::Value::Dict debug_log_;

  base::WeakPtrFactory<ManifestUpdateFinalizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_FINALIZE_COMMAND_H_
