// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"
#include "chrome/browser/web_applications/commands/compute_app_size_command.h"
#include "chrome/browser/web_applications/commands/dedupe_install_urls_command.h"
#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"
#include "chrome/browser/web_applications/commands/fetch_install_info_from_install_url_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/install_app_locally_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/commands/install_from_sync_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/navigate_and_trigger_install_dialog_command.h"
#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/commands/update_protocol_handler_approval_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
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
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/jobs/link_capturing.h"
#endif

namespace web_app {

WebAppCommandScheduler::WebAppCommandScheduler(Profile& profile)
    : profile_(profile) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::SetProvider(base::PassKey<WebAppProvider>,
                                         WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppCommandScheduler::Shutdown() {
  is_in_shutdown_ = true;
}

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), webapps::AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          install_surface, std::move(contents), std::move(dialog_callback),
          std::move(callback), use_fallback,
          provider_->ui_manager().GetWeakPtr(),
          provider_->web_contents_manager().CreateDataRetriever()),
      location);
}

void WebAppCommandScheduler::FetchInstallInfoFromInstallUrl(
    webapps::ManifestId manifest_id,
    GURL install_url,
    webapps::ManifestId parent_manifest_id,
    base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchInstallInfoFromInstallUrlCommand>(
          std::move(manifest_id), std::move(install_url),
          std::move(parent_manifest_id), std::move(callback)));
}

void WebAppCommandScheduler::InstallFromInfo(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback), webapps::AppId(),
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
        FROM_HERE, base::BindOnce(std::move(install_callback), webapps::AppId(),
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
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code,
                            bool did_uninstall_and_replace)> install_callback,
    const WebAppInstallParams& install_params,
    const std::vector<webapps::AppId>& apps_to_uninstall,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(install_callback), webapps::AppId(),
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
    absl::optional<webapps::AppId> installed_placeholder_app_id,
    ExternalAppResolutionCommand::InstalledCallback installed_callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(installed_callback),
                       web_app::ExternallyManagedAppManager::InstallResult(
                           webapps::InstallResultCode::
                               kCancelledOnWebAppProviderShuttingDown,
                           webapps::AppId(),
                           /*did_uninstall_and_replace=*/false)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ExternalAppResolutionCommand>(
          *profile_, external_install_options,
          std::move(installed_placeholder_app_id),
          std::move(installed_callback)),
      location);
}

void WebAppCommandScheduler::PersistFileHandlersUserChoice(
    const webapps::AppId& app_id,
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
    const webapps::AppId& app_id,
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
          provider_->web_contents_manager().CreateDataRetriever(),
          provider_->web_contents_manager().CreateIconDownloader()),
      location);
}

void WebAppCommandScheduler::ScheduleManifestUpdateFinalize(
    const GURL& url,
    const webapps::AppId& app_id,
    WebAppInstallInfo install_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    ManifestWriteCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*url=*/GURL(),
                                  /*app_id=*/webapps::AppId(),
                                  ManifestUpdateResult::kWebContentsDestroyed));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateFinalizeCommand>(
          url, app_id, std::move(install_info), std::move(callback),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive)),
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
          url, web_contents,
          provider_->web_contents_manager().CreateUrlLoader(),
          provider_->web_contents_manager().CreateDataRetriever(),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::ScheduleNavigateAndTriggerInstallDialog(
    const GURL& install_url,
    const GURL& origin,
    bool is_renderer_initiated,
    NavigateAndTriggerInstallDialogCommandCallback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       NavigateAndTriggerInstallDialogCommandResult::kFailure));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<NavigateAndTriggerInstallDialogCommand>(
          install_url, origin, is_renderer_initiated, std::move(callback),
          provider_->ui_manager().GetWeakPtr(),
          std::make_unique<WebAppUrlLoader>(),
          std::make_unique<WebAppDataRetriever>(), &*profile_),
      location);
}

void WebAppCommandScheduler::InstallIsolatedWebApp(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    const absl::optional<base::Version>& expected_version,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    InstallIsolatedWebAppCallback callback,
    const base::Location& call_location) {
  CHECK(optional_profile_keep_alive == nullptr ||
        optional_profile_keep_alive->profile() == &*profile_);

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
          url_info, location, expected_version,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_->GetPrefs()))),
      call_location);
}

void WebAppCommandScheduler::PrepareAndStoreIsolatedWebAppUpdate(
    const IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo& update_info,
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(IsolatedWebAppUpdatePrepareAndStoreCommandResult)>
        callback,
    const base::Location& call_location) {
  if (IsShuttingDown()) {
    IsolatedWebAppUpdatePrepareAndStoreCommandError error{
        .message = "The profile and/or browser are shutting down."};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppUpdatePrepareAndStoreCommand>(
          update_info, url_info,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_->GetPrefs()))),
      call_location);
}

void WebAppCommandScheduler::ApplyPendingIsolatedWebAppUpdate(
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(
        base::expected<void, IsolatedWebAppApplyUpdateCommandError>)> callback,
    const base::Location& call_location) {
  if (IsShuttingDown()) {
    IsolatedWebAppApplyUpdateCommandError error{
        .message = "The profile and/or browser are shutting down."};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppApplyUpdateCommand>(
          url_info,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_->GetPrefs()))),
      call_location);
}

// Given the |bundle_metadata| of a Signed Web Bundle, schedules a command to
// check the installability of the bundle.
void WebAppCommandScheduler::CheckIsolatedWebAppBundleInstallability(
    const SignedWebBundleMetadata& bundle_metadata,
    base::OnceCallback<void(CheckIsolatedWebAppBundleInstallabilityCommand::
                                InstallabilityCheckResult,
                            absl::optional<base::Version>)> callback,
    const base::Location& call_location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       CheckIsolatedWebAppBundleInstallabilityCommand::
                           InstallabilityCheckResult::kShutdown,
                       /*installed_version=*/
                       absl::nullopt));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CheckIsolatedWebAppBundleInstallabilityCommand>(
          &profile_.get(), bundle_metadata, std::move(callback)),
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

void WebAppCommandScheduler::GetControlledFramePartition(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(absl::optional<content::StoragePartitionConfig>)>
        callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<AppLock>(
      "GetControlledFramePartition",
      std::make_unique<AppLockDescription>(url_info.app_id()),
      base::BindOnce(&GetControlledFramePartitionWithLock, &profile_.get(),
                     url_info, partition_name, in_memory, std::move(callback)),
      location);
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
      std::make_unique<InstallFromSyncCommand>(&profile_.get(), params,
                                               std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveInstallUrl(
    absl::optional<webapps::AppId> app_id,
    WebAppManagement::Type install_source,
    const GURL& install_url,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kCancelled));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          std::make_unique<RemoveInstallUrlJob>(uninstall_source, *profile_,
                                                std::move(app_id),
                                                install_source, install_url),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveInstallSource(
    const webapps::AppId& app_id,
    WebAppManagement::Type install_source,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kCancelled));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          std::make_unique<RemoveInstallSourceJob>(uninstall_source, *profile_,
                                                   app_id, install_source),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::UninstallWebApp(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kCancelled));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          std::make_unique<RemoveWebAppJob>(uninstall_source, *profile_,
                                            app_id),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::UninstallAllUserInstalledWebApps(
    webapps::WebappUninstallSource uninstall_source,
    UninstallAllUserInstalledWebAppsCommand::Callback callback,
    const base::Location& location) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       ConvertUninstallResultCodeToString(
                           webapps::UninstallResultCode::kCancelled)));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          uninstall_source, *profile_, std::move(callback)),
      location);
}

void WebAppCommandScheduler::SetRunOnOsLoginMode(
    const webapps::AppId& app_id,
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
    const webapps::AppId& app_id,
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
    const webapps::AppId& app_id,
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

void WebAppCommandScheduler::SetAppIsDisabled(const webapps::AppId& app_id,
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
                       base::flat_set<webapps::AppId>>({app_id}),
      base::BindOnce(
          [](const webapps::AppId& app_id, bool is_disabled,
             web_app::AppLock& lock) {
            lock.sync_bridge().SetAppIsDisabled(lock, app_id, is_disabled);
          },
          app_id, is_disabled),
      location);
}

void WebAppCommandScheduler::ComputeAppSize(
    const webapps::AppId& app_id,
    base::OnceCallback<void(absl::optional<ComputeAppSizeCommand::Size>)>
        callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ComputeAppSizeCommand>(app_id, &profile_.get(),
                                              std::move(callback)));
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
    const base::Location& location,
    base::OnceClosure on_complete) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CallbackCommand<LockType>>(
          operation_name, std::move(lock_description), std::move(callback),
          std::move(on_complete)),
      location);
}

void WebAppCommandScheduler::LaunchApp(
    const webapps::AppId& app_id,
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

void WebAppCommandScheduler::LaunchUrlInApp(const webapps::AppId& app_id,
                                            const GURL& url,
                                            LaunchWebAppCallback callback,
                                            const base::Location& location) {
  CHECK(url.is_valid());
  apps::AppLaunchParams params =
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          app_id, *base::CommandLine::ForCurrentProcess(),
          /*current_directory=*/base::FilePath(),
          /*url_handler_launch_url=*/absl::nullopt,
          /*protocol_handler_launch_url=*/absl::nullopt,
          /*file_launch_url=*/absl::nullopt, /*launch_files=*/{});
  params.override_url = url;

  LaunchApp(std::move(params),
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

void WebAppCommandScheduler::InstallAppLocally(const webapps::AppId& app_id,
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

void WebAppCommandScheduler::SynchronizeOsIntegration(
    const webapps::AppId& app_id,
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

void WebAppCommandScheduler::ScheduleDedupeInstallUrls(
    base::OnceClosure callback,
    const base::Location& location) {
  CHECK(base::FeatureList::IsEnabled(features::kWebAppDedupeInstallUrls));

  base::UmaHistogramCounts100("WebApp.DedupeInstallUrls.SessionRunCount",
                              ++dedupe_install_urls_run_count_);

  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<DedupeInstallUrlsCommand>(profile_.get(),
                                                 std::move(callback)),
      location);
}

void WebAppCommandScheduler::SetAppCapturesSupportedLinksDisableOverlapping(
    const webapps::AppId app_id,
    bool set_to_preferred,
    base::OnceClosure done,
    const base::Location& location) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Preferred apps in ChromeOS are implemented in AppService";
#else
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(done));
    return;
  }

  ScheduleCallbackWithLock(
      "SetAppCapturesSupporedLinks", std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(::web_app::SetAppCapturesSupportedLinksDisableOverlapping,
                     app_id, set_to_preferred),
      location, std::move(done));
#endif
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
  webapps::AppId app_id = params.app_id;
  ScheduleCallbackWithLock(
      "LaunchApp", std::make_unique<AppLockDescription>(app_id),
      base::BindOnce(&WebAppUiManager::WaitForFirstRunAndLaunchWebApp,
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
    const base::Location& location,
    base::OnceClosure on_complete);

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
    const base::Location& location,
    base::OnceClosure on_complete);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<void(AppLock& lock)> callback,
    const base::Location& location);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(AppLock& lock)> callback,
    const base::Location& location,
    base::OnceClosure on_complete);

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
    const base::Location& location,
    base::OnceClosure on_complete);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<AllAppsLock>(
    const std::string& operation_name,
    std::unique_ptr<AllAppsLock::LockDescription> lock_description,
    base::OnceCallback<void(AllAppsLock& lock)> callback,
    const base::Location& location);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<AllAppsLock>(
    const std::string& operation_name,
    std::unique_ptr<AllAppsLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(AllAppsLock& lock)> callback,
    const base::Location& location,
    base::OnceClosure on_complete);

}  // namespace web_app
