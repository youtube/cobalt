// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_

#include <map>

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppShortcutManager;
class WebAppFileHandlerManager;
class WebAppProtocolHandlerManager;
class UrlHandlerManager;

class FakeOsIntegrationManager : public OsIntegrationManager {
 public:
  FakeOsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
      std::unique_ptr<UrlHandlerManager> url_handler_manager);
  ~FakeOsIntegrationManager() override;

  // OsIntegrationManager:
  void InstallOsHooks(const webapps::AppId& app_id,
                      InstallOsHooksCallback callback,
                      std::unique_ptr<WebAppInstallInfo> web_app_info,
                      InstallOsHooksOptions options) override;
  void UninstallOsHooks(const webapps::AppId& app_id,
                        const OsHooksOptions& os_hooks,
                        UninstallOsHooksCallback callback) override;
  void UninstallAllOsHooks(const webapps::AppId& app_id,
                           UninstallOsHooksCallback callback) override;
  void UpdateOsHooks(const webapps::AppId& app_id,
                     base::StringPiece old_name,
                     FileHandlerUpdateAction file_handlers_need_os_update,
                     const WebAppInstallInfo& web_app_info,
                     UpdateOsHooksCallback callback) override;

  // FakeOsIntegrationManager skips the execution logic and writes directly
  // to the DB, even if the execute_and_write_config
  // param is enabled in features::kOsIntegrationSubManagers. To test the
  // actual OS integration, use the production version of OsIntegrationManager.
  //
  // See OsIntegrationSynchronizeCommandTest as an example of using this
  // function.
  void Synchronize(const webapps::AppId& app_id,
                   base::OnceClosure callback,
                   absl::optional<SynchronizeOsOptions> options) override;

  size_t num_create_shortcuts_calls() const {
    return num_create_shortcuts_calls_;
  }

  size_t num_create_file_handlers_calls() const {
    return num_create_file_handlers_calls_;
  }

  size_t num_update_file_handlers_calls() const {
    return num_update_file_handlers_calls_;
  }

  size_t num_register_run_on_os_login_calls() const {
    return num_register_run_on_os_login_calls_;
  }

  size_t num_unregister_run_on_os_login_calls() const {
    return num_unregister_run_on_os_login_calls_;
  }

  size_t num_add_app_to_quick_launch_bar_calls() const {
    return num_add_app_to_quick_launch_bar_calls_;
  }

  size_t num_register_url_handlers_calls() const {
    return num_register_url_handlers_calls_;
  }

  void set_can_create_shortcuts(bool can_create_shortcuts) {
    can_create_shortcuts_ = can_create_shortcuts;
  }

  absl::optional<bool> did_add_to_desktop() const {
    return did_add_to_desktop_;
  }

  absl::optional<InstallOsHooksOptions> get_last_install_options() const {
    return last_options_;
  }

  void SetNextCreateShortcutsResult(const webapps::AppId& app_id, bool success);

  void SetFileHandlerManager(
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager);

  void SetUrlHandlerManager(
      std::unique_ptr<UrlHandlerManager> url_handler_manager);

  void SetShortcutManager(
      std::unique_ptr<WebAppShortcutManager> shortcut_manager);

  FakeOsIntegrationManager* AsTestOsIntegrationManager() override;

 private:
  size_t num_create_shortcuts_calls_ = 0;
  size_t num_create_file_handlers_calls_ = 0;
  size_t num_update_file_handlers_calls_ = 0;
  size_t num_register_run_on_os_login_calls_ = 0;
  size_t num_unregister_run_on_os_login_calls_ = 0;
  size_t num_add_app_to_quick_launch_bar_calls_ = 0;
  size_t num_register_url_handlers_calls_ = 0;
  absl::optional<bool> did_add_to_desktop_;
  absl::optional<InstallOsHooksOptions> last_options_;

  bool can_create_shortcuts_ = true;
  std::map<webapps::AppId, bool> next_create_shortcut_results_;
};

// Stub test shortcut manager.
class TestShortcutManager : public WebAppShortcutManager {
 public:
  explicit TestShortcutManager(Profile* profile);
  ~TestShortcutManager() override;
  std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const webapps::AppId& app_id) override;
  void SetShortcutInfoForApp(const webapps::AppId& app_id,
                             std::unique_ptr<ShortcutInfo> shortcut_info);
  void GetShortcutInfoForApp(const webapps::AppId& app_id,
                             GetShortcutInfoCallback callback) override;
  void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info) override;
  void SetAppExistingShortcuts(const GURL& app_url,
                               ShortcutLocations locations) {
    existing_shortcut_locations_[app_url] = locations;
  }

  std::map<webapps::AppId, std::unique_ptr<ShortcutInfo>> shortcut_info_map_;
  std::map<GURL, ShortcutLocations> existing_shortcut_locations_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_OS_INTEGRATION_MANAGER_H_
