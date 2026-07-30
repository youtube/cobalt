// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

namespace {

bool IsAppInstalled(ExtensionsManager& extensions_manager,
                    const WebAppRegistrar& registrar,
                    const webapps::AppId& app_id) {
  if (registrar.GetInstallState(app_id).has_value()) {
    return true;
  }
  return extensions_manager.IsExtensionInstalled(app_id);
}

}  // namespace

WebAppUninstallAndReplaceJob::WebAppUninstallAndReplaceJob(
    Profile* profile,
    base::DictValue& debug_value,
    WithAppResources& to_app_lock,
    const std::vector<webapps::AppId>& from_apps_or_extensions,
    const webapps::AppId& to_app,
    base::OnceCallback<void(bool uninstall_triggered)> on_complete)
    : profile_(*profile),
      debug_value_(debug_value),
      to_app_lock_(to_app_lock),
      from_apps_or_extensions_(from_apps_or_extensions),
      to_app_(to_app),
      on_complete_(std::move(on_complete)) {}
WebAppUninstallAndReplaceJob::~WebAppUninstallAndReplaceJob() = default;

void WebAppUninstallAndReplaceJob::Start() {
  CHECK(to_app_lock_->registrar().GetAppById(to_app_));

  std::vector<webapps::AppId> apps_to_replace;
  for (const webapps::AppId& from_app : from_apps_or_extensions_) {
    if (IsAppInstalled(to_app_lock_->extensions_manager(),
                       to_app_lock_->registrar(), from_app)) {
      apps_to_replace.emplace_back(from_app);
    }
  }

  if (apps_to_replace.empty()) {
    debug_value_->Set("did_uninstall_and_replace", false);
    std::move(on_complete_).Run(/*uninstall_triggered=*/false);
    return;
  }

  debug_value_->Set("did_uninstall_and_replace", true);
  MigrateUiAndUninstallApp(
      apps_to_replace.front(),
      base::BindOnce(std::move(on_complete_), /*uninstall_triggered=*/true));

  apps_to_replace.erase(apps_to_replace.begin());
  for (const auto& app : apps_to_replace) {
    to_app_lock_->ui_manager().UninstallAppSilentlyForMigration(app);
  }
}

void WebAppUninstallAndReplaceJob::MigrateUiAndUninstallApp(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete) {
#if BUILDFLAG(IS_CHROMEOS)
  to_app_lock_->ui_manager().MigrateLauncherState(
      from_app, to_app_,
      base::BindOnce(&WebAppUninstallAndReplaceJob::OnMigrateLauncherState,
                     weak_ptr_factory_.GetWeakPtr(), from_app,
                     std::move(on_complete)));
#else
  OnMigrateLauncherState(from_app, std::move(on_complete));
#endif
}

void WebAppUninstallAndReplaceJob::OnMigrateLauncherState(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete) {
  // If migration of user/UI data is required for other app types consider
  // generalising this operation to be part of app service.
  ExtensionsManager& extensions_manager = to_app_lock_->extensions_manager();

  if (extensions_manager.IsExtensionInstalled(from_app)) {
    extensions_manager.CopyAppSortingLayout(from_app, to_app_);

    SetUserDisplayModeCommand::DoSetDisplayMode(
        *to_app_lock_, to_app_,
        extensions_manager.GetExtensionUserDisplayMode(from_app),
        /*is_user_action=*/false);

    auto shortcut_info = extensions_manager.GetExtensionShortcutInfo(from_app);
    to_app_lock_->os_integration_manager().GetAppExistingShortCutLocation(
        base::BindOnce(
            &WebAppUninstallAndReplaceJob::OnShortcutLocationGathered,
            weak_ptr_factory_.GetWeakPtr(), from_app, std::move(on_complete)),
        std::move(shortcut_info));
  } else {
    // The from_app could be a web app.
    to_app_lock_->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
        from_app,
        base::BindOnce(&WebAppUninstallAndReplaceJob::
                           OnShortcutInfoReceivedSearchShortcutLocations,
                       weak_ptr_factory_.GetWeakPtr(), from_app,
                       std::move(on_complete)));
  }
}

void WebAppUninstallAndReplaceJob::
    OnShortcutInfoReceivedSearchShortcutLocations(
        const webapps::AppId& from_app,
        base::OnceClosure on_complete,
        std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (!shortcut_info) {
    to_app_lock_->ui_manager().UninstallAppSilentlyForMigration(from_app);
    std::move(on_complete).Run();
    return;
  }

  auto callback = base::BindOnce(
      &WebAppUninstallAndReplaceJob::OnShortcutLocationGathered,
      weak_ptr_factory_.GetWeakPtr(), from_app, std::move(on_complete));
  to_app_lock_->os_integration_manager().GetAppExistingShortCutLocation(
      std::move(callback), std::move(shortcut_info));
}

void WebAppUninstallAndReplaceJob::OnShortcutLocationGathered(
    const webapps::AppId& from_app,
    base::OnceClosure on_complete,
    ShortcutLocations from_app_locations) {
  ExtensionsManager& extensions_manager = to_app_lock_->extensions_manager();

  const bool is_extension = extensions_manager.IsExtensionInstalled(from_app);
  bool run_on_os_login = from_app_locations.in_startup;
  if (is_extension) {
    // Need to be called before `UninstallAppSilently` because
    // UninstallAppSilently might synchronously finish, so the wait won't get
    // finished if called after.
    extensions_manager.WaitForExtensionShortcutsDeleted(
        from_app,
        base::BindOnce(&WebAppUninstallAndReplaceJob::
                           SynchronizeOSIntegrationForReplacementApp,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_complete),
                       run_on_os_login, from_app_locations));
  } else {
    // Platforms like Mac don't fetch the 'run on os login' property from the
    // GetAppExistingShortCutLocation API.
    run_on_os_login =
        run_on_os_login ||
        to_app_lock_->registrar().GetAppRunOnOsLoginMode(from_app).value ==
            RunOnOsLoginMode::kWindowed;
  }

  // When the `from_app` is a web app, we can't wait for it to finish because it
  // underlying schedules WebAppUninstallCommand which uses a `AllAppsLock`,
  // so the uninstall command won't get started until current command that holds
  // the `to_app_lock` finishes.
  to_app_lock_->ui_manager().UninstallAppSilentlyForMigration(from_app);

  if (!is_extension) {
    SynchronizeOSIntegrationForReplacementApp(
        std::move(on_complete), run_on_os_login, from_app_locations);
  }
}

void WebAppUninstallAndReplaceJob::SynchronizeOSIntegrationForReplacementApp(
    base::OnceClosure on_complete,
    bool from_app_run_on_os_login,
    ShortcutLocations from_app_locations) {
  ValueWithPolicy<RunOnOsLoginMode> run_on_os_login =
      to_app_lock_->registrar().GetAppRunOnOsLoginMode(to_app_);
  if (run_on_os_login.user_controllable) {
    RunOnOsLoginMode new_mode = from_app_run_on_os_login
                                    ? RunOnOsLoginMode::kWindowed
                                    : RunOnOsLoginMode::kNotRun;
    if (new_mode != run_on_os_login.value) {
      {
        ScopedRegistryUpdate update = to_app_lock_->sync_bridge().BeginUpdate();
        update->UpdateApp(to_app_)->SetRunOnOsLoginMode(new_mode);
      }
    }
  }

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = from_app_locations.on_desktop;
  synchronize_options.add_to_quick_launch_bar =
      from_app_locations.in_quick_launch_bar;
  synchronize_options.reason = SHORTCUT_CREATION_AUTOMATED;
  to_app_lock_->os_integration_manager().Synchronize(to_app_,
                                                     std::move(on_complete));
}

}  // namespace web_app
