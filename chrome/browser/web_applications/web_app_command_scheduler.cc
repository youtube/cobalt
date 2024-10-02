// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/install_app_locally_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/commands/install_from_sync_command.h"
#include "chrome/browser/web_applications/commands/install_placeholder_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/commands/update_protocol_handler_approval_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {
namespace {

std::unique_ptr<content::WebContents> CreateIsolatedWebAppWebContents(
    Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          /*context=*/&profile));

  webapps::InstallableManager::CreateForWebContents(web_contents.get());

  return web_contents;
}

}  // namespace

WebAppCommandScheduler::WebAppCommandScheduler(Profile& profile,
                                               WebAppProvider* provider)
    : profile_(profile),
      provider_(provider),
      url_loader_(std::make_unique<WebAppUrlLoader>()) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::Shutdown() {
  is_in_shutdown_ = true;
}

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          install_surface, std::move(contents), bypass_service_worker_check,
          std::move(dialog_callback), std::move(callback), use_fallback,
          std::make_unique<WebAppDataRetriever>()),
      location);
}

void WebAppCommandScheduler::InstallFromInfo(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          &profile_.get(), std::move(install_info),
          overwrite_existing_manifest_fields, install_surface,
          std::move(install_callback)),
      location);
}

void WebAppCommandScheduler::InstallFromInfoWithParams(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const WebAppInstallParams& install_params,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          &profile_.get(), std::move(install_info),
          overwrite_existing_manifest_fields, install_surface,
          std::move(install_callback), install_params),
      location);
}

void WebAppCommandScheduler::InstallFromInfoWithParams(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    base::OnceCallback<void(const AppId& app_id,
                            webapps::InstallResultCode code,
                            bool did_uninstall_and_replace)> install_callback,
    const WebAppInstallParams& install_params,
    const std::vector<AppId>& apps_to_uninstall,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(install_callback), AppId(),
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown,
            /*did_uninstall_and_replace=*/false));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          &profile_.get(), std::move(install_info),
          overwrite_existing_manifest_fields, std::move(install_surface),
          std::move(install_callback), install_params, apps_to_uninstall),
      location);
}

void WebAppCommandScheduler::InstallExternallyManagedApp(
    const ExternalInstallOptions& external_install_options,
    base::OnceCallback<void(const AppId& app_id,
                            webapps::InstallResultCode code,
                            bool did_uninstall_and_replace)> install_callback,
    base::WeakPtr<content::WebContents> contents,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    WebAppUrlLoader* web_app_url_loader,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(install_callback), AppId(),
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown,
            /*did_uninstall_and_replace=*/false));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          &profile_.get(), external_install_options,
          std::move(install_callback), contents, std::move(data_retriever),
          web_app_url_loader),
      location);
}

void WebAppCommandScheduler::InstallPlaceholder(
    const ExternalInstallOptions& install_options,
    base::OnceCallback<void(const AppId& app_id,
                            webapps::InstallResultCode code,
                            bool did_uninstall_and_replace)> callback,
    base::WeakPtr<content::WebContents> web_contents,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), AppId(),
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown,
            /*did_uninstall_and_replace=*/false));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallPlaceholderCommand>(
          &profile_.get(), install_options, std::move(callback), web_contents,
          std::make_unique<WebAppDataRetriever>()),
      location);
}

void WebAppCommandScheduler::PersistFileHandlersUserChoice(
    const AppId& app_id,
    bool allowed,
    base::OnceClosure callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      UpdateFileHandlerCommand::CreateForPersistUserChoice(app_id, allowed,
                                                           std::move(callback)),
      location);
}

void WebAppCommandScheduler::ScheduleManifestUpdateCheck(
    const GURL& url,
    const AppId& app_id,
    base::Time check_time,
    base::WeakPtr<content::WebContents> contents,
    ManifestUpdateCheckCommand::CompletedCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ManifestUpdateCheckResult::kSystemShutdown,
                                  /*install_info_=*/absl::nullopt));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateCheckCommand>(
          url, app_id, check_time, contents, std::move(callback),
          std::make_unique<WebAppDataRetriever>()),
      location);
}

void WebAppCommandScheduler::ScheduleManifestUpdateFinalize(
    const GURL& url,
    const AppId& app_id,
    WebAppInstallInfo install_info,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    ManifestWriteCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*url=*/GURL(),
                                  /*app_id=*/AppId(),
                                  ManifestUpdateResult::kWebContentsDestroyed));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateFinalizeCommand>(
          url, app_id, std::move(install_info), std::move(callback),
          std::move(keep_alive), std::move(profile_keep_alive)),
      location);
}

void WebAppCommandScheduler::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    FetchInstallabilityForChromeManagementCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  InstallableCheckResult::kNotInstallable,
                                  /*app_id=*/absl::nullopt));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<web_app::FetchInstallabilityForChromeManagement>(
          url, web_contents, std::make_unique<web_app::WebAppUrlLoader>(),
          std::make_unique<web_app::WebAppDataRetriever>(),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::InstallIsolatedWebApp(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    InstallIsolatedWebAppCallback callback,
    const base::Location& call_location) {
  DCHECK(profile_keep_alive == nullptr ||
         profile_keep_alive->profile() == &*profile_);

  if (IsShuttingDown()) {
    InstallIsolatedWebAppCommandError error;
    error.message = "The profile and/or browser are shutting down.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedWebAppCommand>(
          url_info, location, CreateIsolatedWebAppWebContents(*profile_),
          std::make_unique<WebAppUrlLoader>(), std::move(keep_alive),
          std::move(profile_keep_alive), std::move(callback),
          InstallIsolatedWebAppCommand::CreateDefaultResponseReaderFactory(
              *profile_->GetPrefs())),
      call_location);
}

void WebAppCommandScheduler::GetIsolatedWebAppBrowsingData(
    base::OnceCallback<void(base::flat_map<url::Origin, int64_t>)> callback,
    const base::Location& call_location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::flat_map<url::Origin, int64_t>()));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<GetIsolatedWebAppBrowsingDataCommand>(
          &profile_.get(), std::move(callback)),
      call_location);
}

void WebAppCommandScheduler::InstallFromSync(const WebApp& web_app,
                                             OnceInstallCallback callback,
                                             const base::Location& location) {
  DCHECK(web_app.is_from_sync_and_pending_installation());
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      web_app.app_id(), web_app.manifest_id(), web_app.start_url(),
      web_app.sync_fallback_data().name, web_app.sync_fallback_data().scope,
      web_app.sync_fallback_data().theme_color, web_app.user_display_mode(),
      web_app.sync_fallback_data().icon_infos);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(
          url_loader_.get(), &profile_.get(),
          std::make_unique<WebAppDataRetriever>(), params, std::move(callback)),
      location);
}

void WebAppCommandScheduler::Uninstall(
    const AppId& app_id,
    absl::optional<WebAppManagement::Type> external_install_source,
    webapps::WebappUninstallSource uninstall_source,
    WebAppUninstallCommand::UninstallWebAppCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kCancelled));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, external_install_source, uninstall_source,
          std::move(callback), &profile_.get()),
      location);
}

void WebAppCommandScheduler::SetRunOnOsLoginMode(
    const AppId& app_id,
    RunOnOsLoginMode login_mode,
    base::OnceClosure callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(app_id, login_mode,
                                                 std::move(callback)),
      location);
}

void WebAppCommandScheduler::SyncRunOnOsLoginMode(
    const AppId& app_id,
    base::OnceClosure callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode(app_id, std::move(callback)),
      location);
}

void WebAppCommandScheduler::UpdateProtocolHandlerUserApproval(
    const AppId& app_id,
    const std::string& protocol_scheme,
    ApiApprovalState approval_state,
    base::OnceClosure callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<UpdateProtocolHandlerApprovalCommand>(
          app_id, protocol_scheme, approval_state, std::move(callback)),
      location);
}

void WebAppCommandScheduler::ClearWebAppBrowsingData(
    const base::Time& begin_time,
    const base::Time& end_time,
    base::OnceClosure done,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(done));
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<AllAppsLock>(
      "ClearWebAppBrowsingData", std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(web_app::ClearWebAppBrowsingData, begin_time, end_time,
                     std::move(done)),
      location);
}

void WebAppCommandScheduler::SetAppIsDisabled(const AppId& app_id,
                                              bool is_disabled,
                                              base::OnceClosure callback,
                                              const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<web_app::AppLock>(
      "SetAppIsDisabled",
      std::make_unique<web_app::AppLockDescription,
                       base::flat_set<web_app::AppId>>({app_id}),
      base::BindOnce(
          [](const web_app::AppId& app_id, bool is_disabled,
             web_app::AppLock& lock) {
            lock.sync_bridge().SetAppIsDisabled(lock, app_id, is_disabled);
          },
          app_id, is_disabled),
      location);
}

template <class LockType, class DescriptionType>
void WebAppCommandScheduler::ScheduleCallbackWithLock(
    const std::string& operation_name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<void(LockType& lock)> callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CallbackCommand<LockType>>(
          operation_name, std::move(lock_description), std::move(callback)),
      location);
}

template <class LockType, class DescriptionType>
void WebAppCommandScheduler::ScheduleCallbackWithLock(
    const std::string& operation_name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<base::Value(LockType& lock)> callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CallbackCommand<LockType>>(
          operation_name, std::move(lock_description), std::move(callback)),
      location);
}

void WebAppCommandScheduler::LaunchApp(
    const AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const absl::optional<GURL>& url_handler_launch_url,
    const absl::optional<GURL>& protocol_handler_launch_url,
    const absl::optional<GURL>& file_launch_url,
    const std::vector<base::FilePath>& launch_files,
    LaunchWebAppCallback callback,
    const base::Location& location) {
  LaunchApp(WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
                app_id, command_line, current_directory, url_handler_launch_url,
                protocol_handler_launch_url, file_launch_url, launch_files),
            LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
            std::move(callback), location);
}

void WebAppCommandScheduler::LaunchAppWithCustomParams(
    apps::AppLaunchParams params,
    LaunchWebAppCallback callback,
    const base::Location& location) {
  LaunchApp(std::move(params), LaunchWebAppWindowSetting::kUseLaunchParams,
            std::move(callback), location);
}

void WebAppCommandScheduler::SynchronizeOsIntegration(
    const AppId& app_id,
    base::OnceClosure synchronize_callback,
    absl::optional<SynchronizeOsOptions> synchronize_options,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(synchronize_callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<OsIntegrationSynchronizeCommand>(
          app_id, synchronize_options, std::move(synchronize_callback)),
      location);
}

void WebAppCommandScheduler::InstallAppLocally(const AppId& app_id,
                                               base::OnceClosure callback,
                                               const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallAppLocallyCommand>(app_id, std::move(callback)),
      location);
}

void WebAppCommandScheduler::LaunchApp(apps::AppLaunchParams params,
                                       LaunchWebAppWindowSetting option,
                                       LaunchWebAppCallback callback,
                                       const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr, nullptr,
                                  apps::LaunchContainer::kLaunchContainerNone));
    return;
  }
  // Off the record profiles cannot be 'kept alive'.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive =
      profile_->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ScopedProfileKeepAlive>(
                &profile_.get(), ProfileKeepAliveOrigin::kAppWindow);
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::WEB_APP_LAUNCH,
                                        KeepAliveRestartOption::ENABLED);

  auto launch_with_keep_alives =
      base::BindOnce(&WebAppCommandScheduler::LaunchAppWithKeepAlives,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params), option,
                     std::move(callback), std::move(profile_keep_alive),
                     std::move(browser_keep_alive), location);
  // Because we are accessing the WebAppUiManager, we should wait until the
  // provider has started to actually create the command.
  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(FROM_HERE,
                                        std::move(launch_with_keep_alives));
    return;
  }
  std::move(launch_with_keep_alives).Run();
}

void WebAppCommandScheduler::LaunchAppWithKeepAlives(
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    LaunchWebAppCallback callback,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
    const base::Location& location) {
  DCHECK(provider_->is_registry_ready());

  // Decorate the callback to ensure the keep alives are kept alive during the
  // execution of the launch.
  callback = std::move(callback).Then(base::BindOnce(
      [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
         std::unique_ptr<ScopedKeepAlive> browser_keep_alive) {},
      std::move(profile_keep_alive), std::move(browser_keep_alive)));

  // Unretained is safe because this callback is lives on the WebAppProvider
  // (via the WebAppCommandManager), which is a Profile KeyedService. It is
  // destructed when the profile is shutting down as well. So it is impossible
  // for this callback to be run with the WebAppUiManager being destructed.
  AppId app_id = params.app_id;
  ScheduleCallbackWithLock(
      "LaunchApp", std::make_unique<AppLockDescription>(app_id),
      base::BindOnce(&WebAppUiManager::LaunchWebApp,
                     base::Unretained(&provider_->ui_manager()),
                     std::move(params), launch_setting, std::ref(*profile_),
                     std::move(callback)),
      location);
}

bool WebAppCommandScheduler::IsShuttingDown() const {
  return is_in_shutdown_ ||
         KeepAliveRegistry::GetInstance()->IsShuttingDown() ||
         profile_->ShutdownStarted();
}

template void WebAppCommandScheduler::ScheduleCallbackWithLock<NoopLock>(
    const std::string& operation_name,
    std::unique_ptr<NoopLock::LockDescription> lock_description,
    base::OnceCallback<void(NoopLock& lock)> callback,
    const base::Location& location);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<NoopLock>(
    const std::string& operation_name,
    std::unique_ptr<NoopLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(NoopLock& lock)> callback,
    const base::Location& location);

template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsLock::LockDescription> lock_description,
    base::OnceCallback<void(SharedWebContentsLock& lock)> callback,
    const base::Location& location);
template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(SharedWebContentsLock& lock)> callback,
    const base::Location& location);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<void(AppLock& lock)> callback,
    const base::Location& location);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(AppLock& lock)> callback,
    const base::Location& location);

template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsWithAppLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsWithAppLock::LockDescription>
        lock_description,
    base::OnceCallback<void(SharedWebContentsWithAppLock& lock)> callback,
    const base::Location& location);
template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsWithAppLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsWithAppLock::LockDescription>
        lock_description,
    base::OnceCallback<base::Value(SharedWebContentsWithAppLock& lock)>
        callback,
    const base::Location& location);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<AllAppsLock>(
    const std::string& operation_name,
    std::unique_ptr<AllAppsLock::LockDescription> lock_description,
    base::OnceCallback<void(AllAppsLock& lock)> callback,
    const base::Location& location);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<AllAppsLock>(
    const std::string& operation_name,
    std::unique_ptr<AllAppsLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(AllAppsLock& lock)> callback,
    const base::Location& location);

}  // namespace web_app
