// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/web_applications/commands/compute_app_size_command.h"
#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/navigate_and_trigger_install_dialog_command.h"
#include "chrome/browser/web_applications/commands/uninstall_all_user_installed_web_apps_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class Profile;

namespace content {
class StoragePartitionConfig;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

struct IsolatedWebAppApplyUpdateCommandError;
class IsolatedWebAppUrlInfo;
class SignedWebBundleMetadata;
class WebApp;
struct WebAppInstallInfo;
class WebAppProvider;
enum class ApiApprovalState;
struct IsolationData;
struct SynchronizeOsOptions;

// The command scheduler is the main API to access the web app system. The
// scheduler internally ensures:
// * Operations occur after the WebAppProvider is ready (so you don't have to
//   manually wait for this).
// * Operations are isolated from other operations in the system (currently
//   implemented using `WebAppCommand`s) to prevent race conditions while
//   reading/writing from the various data storage of the system.
// * Operations have the necessary dependencies from the WebAppProvider system.
class WebAppCommandScheduler {
 public:
  using ManifestWriteCallback =
      ManifestUpdateFinalizeCommand::ManifestWriteCallback;
  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError>)>;

  explicit WebAppCommandScheduler(Profile& profile);
  virtual ~WebAppCommandScheduler();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Shutdown();

  // User initiated install that uses current `WebContents` to fetch manifest
  // and install the web app.
  void FetchManifestAndInstall(webapps::WebappInstallSource install_surface,
                               base::WeakPtr<content::WebContents> contents,
                               WebAppInstallDialogCallback dialog_callback,
                               OnceInstallCallback callback,
                               bool use_fallback,
                               const base::Location& location = FROM_HERE);

  void FetchInstallInfoFromInstallUrl(
      webapps::ManifestId manifest_id,
      GURL install_url,
      webapps::ManifestId parent_manifest_id,
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback);

  // Install with provided `WebAppInstallInfo` instead of fetching data from
  // manifest.
  // `InstallFromInfo` doesn't install OS hooks. `InstallFromInfoWithParams`
  // install OS hooks when they are set in `install_params`.
  void InstallFromInfo(std::unique_ptr<WebAppInstallInfo> install_info,
                       bool overwrite_existing_manifest_fields,
                       webapps::WebappInstallSource install_surface,
                       OnceInstallCallback install_callback,
                       const base::Location& location = FROM_HERE);

  void InstallFromInfoWithParams(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      OnceInstallCallback install_callback,
      const WebAppInstallParams& install_params,
      const base::Location& location = FROM_HERE);

  void InstallFromInfoWithParams(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      base::OnceCallback<void(const webapps::AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)> install_callback,
      const WebAppInstallParams& install_params,
      const std::vector<webapps::AppId>& apps_to_uninstall,
      const base::Location& location = FROM_HERE);

  // Install web apps managed by `ExternallyInstalledAppManager`.
  void InstallExternallyManagedApp(
      const ExternalInstallOptions& external_install_options,
      absl::optional<webapps::AppId> installed_placeholder_app_id,
      ExternalAppResolutionCommand::InstalledCallback installed_callback,
      const base::Location& location = FROM_HERE);

  void PersistFileHandlersUserChoice(
      const webapps::AppId& app_id,
      bool allowed,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Schedule a command that performs fetching data from the manifest
  // for a manifest update.
  void ScheduleManifestUpdateCheck(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> contents,
      ManifestUpdateCheckCommand::CompletedCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that performs the data writes into the DB for
  // completion of the manifest update.
  void ScheduleManifestUpdateFinalize(
      const GURL& url,
      const webapps::AppId& app_id,
      WebAppInstallInfo install_info,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ManifestWriteCallback callback,
      const base::Location& location = FROM_HERE);

  void FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      FetchInstallabilityForChromeManagementCallback callback,
      const base::Location& location = FROM_HERE);

  void ScheduleNavigateAndTriggerInstallDialog(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      NavigateAndTriggerInstallDialogCommandCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that installs the Isolated Web App described by the
  // given IsolatedWebAppUrlInfo and IsolationData. If `expected_version` is
  // set, then this command will refuse to install the Isolated Web App if its
  // version does not match.
  virtual void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location,
      const absl::optional<base::Version>& expected_version,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Schedules a command to prepare the update of an Isolated Web App.
  // `update_info` specifies the location of the update for the IWA referred to
  // in `url_info`. This command is safe to run even if the IWA is not installed
  // or already updated, in which case it will gracefully fail. If a dry-run of
  // the update succeeds, then the `update_info` is persisted in the
  // `IsolationData::pending_update_info()` of the IWA in the Web App database.
  virtual void PrepareAndStoreIsolatedWebAppUpdate(
      const IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo& update_info,
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<void(IsolatedWebAppUpdatePrepareAndStoreCommandResult)>
          callback,
      const base::Location& call_location = FROM_HERE);

  // Schedules a command to apply a prepared pending update of an Isolated Web
  // App. This command is safe to run even if the IWA is not installed or
  // already updated, in which case it will gracefully fail. Regardless of
  // whether the update succeeds or fails, `IsolationData::pending_update_info`
  // of the IWA in the Web App database will be cleared.
  virtual void ApplyPendingIsolatedWebAppUpdate(
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<
          void(base::expected<void, IsolatedWebAppApplyUpdateCommandError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  // Given the |bundle_metadata| of a Signed Web Bundle, schedules a command to
  // check the installability of the bundle.
  virtual void CheckIsolatedWebAppBundleInstallability(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(CheckIsolatedWebAppBundleInstallabilityCommand::
                                  InstallabilityCheckResult,
                              absl::optional<base::Version>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Computes the browsing data size of all installed Isolated Web Apps.
  void GetIsolatedWebAppBrowsingData(
      base::OnceCallback<void(base::flat_map<url::Origin, int64_t>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Registers a <controlledframe>'s StoragePartition with the given Isolated
  // Web App.
  void GetControlledFramePartition(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      bool in_memory,
      base::OnceCallback<void(absl::optional<content::StoragePartitionConfig>)>
          callback,
      const base::Location& location = FROM_HERE);

  // Scheduler a command that installs a web app from sync.
  void InstallFromSync(const WebApp& web_app,
                       OnceInstallCallback callback,
                       const base::Location& location = FROM_HERE);

  // Schedules a command that removes `install_source`'s `install_url` from
  // `app_id`, if `app_id` is unset then the first matching web app that has
  // `install_url` for `install_source` will be used.
  // This will remove the install source if there are no remaining install URLs
  // for that install source which in turn will remove the web app if there are
  // no remaining install sources for the web app.
  // Virtual for testing.
  // TODO(crbug.com/1434692): There could potentially be multiple app matches
  // for `install_source` and `install_url` when `app_id` is not provided,
  // handle this case better than "first matching".
  virtual void RemoveInstallUrl(absl::optional<webapps::AppId> app_id,
                                WebAppManagement::Type install_source,
                                const GURL& install_url,
                                webapps::WebappUninstallSource uninstall_source,
                                UninstallJob::Callback callback,
                                const base::Location& location = FROM_HERE);

  // Schedules a command that removes an install source from a given web app,
  // will uninstall the web app if no install sources remain. May cause a web
  // app to become user uninstallable, will deploy uninstall OS hooks in that
  // case.
  // Virtual for testing.
  virtual void RemoveInstallSource(
      const webapps::AppId& app_id,
      WebAppManagement::Type install_source,
      webapps::WebappUninstallSource uninstall_source,
      UninstallJob::Callback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that removes a web app from the database and cleans up
  // all assets and OS integrations. Disconnects it from any of its sub apps and
  // uninstalls them too if they have no other install sources. Adds the
  // uninstall web app to `UserUninstalledPreinstalledWebAppPrefs` if it was
  // default installed.
  void UninstallWebApp(const webapps::AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       UninstallJob::Callback callback,
                       const base::Location& location = FROM_HERE);

  // Schedules a command that uninstalls all user-installed web apps.
  void UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource uninstall_source,
      UninstallAllUserInstalledWebAppsCommand::Callback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that updates run on os login to provided `login_mode`
  // for a web app.
  void SetRunOnOsLoginMode(const webapps::AppId& app_id,
                           RunOnOsLoginMode login_mode,
                           base::OnceClosure callback,
                           const base::Location& location = FROM_HERE);

  // Schedules a command that syncs the run on os login mode from web app DB to
  // OS.
  void SyncRunOnOsLoginMode(const webapps::AppId& app_id,
                            base::OnceClosure callback,
                            const base::Location& location = FROM_HERE);

  // Updates the approved or disallowed protocol list for the given app. If
  // necessary, it also updates the protocol registration with the OS.
  void UpdateProtocolHandlerUserApproval(
      const webapps::AppId& app_id,
      const std::string& protocol_scheme,
      ApiApprovalState approval_state,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Set app to disabled, This is Chrome OS specific and no-op on other
  // platforms.
  void SetAppIsDisabled(const webapps::AppId& app_id,
                        bool is_disabled,
                        base::OnceClosure callback,
                        const base::Location& location = FROM_HERE);

  // Schedules a command that calculates the app and data size of a web app.
  void ComputeAppSize(
      const webapps::AppId& app_id,
      base::OnceCallback<void(absl::optional<ComputeAppSizeCommand::Size>)>
          callback);

  // Schedules provided callback after `lock` is granted. The callback can
  // access web app resources through the `lock`. The `operation_name` is
  // used describe this operation in the WebAppCommandManager log, surfaced
  // in chrome://web-app-internals for debugging purposes. If the system is
  // shutting down, or has already shut down, then the callback will not be
  // called & will simply be destroyed.
  template <typename LockType,
            typename DescriptionType = typename LockType::LockDescription>
  void ScheduleCallbackWithLock(
      const std::string& operation_name,
      std::unique_ptr<DescriptionType> lock_description,
      base::OnceCallback<void(LockType& lock)> callback,
      const base::Location& location = FROM_HERE);
  // Same as above, with the following diffences:
  // - The callback now returns a debug value that is included in
  //   WebAppCommandManager logs, viewable from chrome://web-app-internals.
  // - An `on_complete` callback argument allows callers to specify a callback
  //   to be called after the command has completed. This is because it can no
  //   longer be simply chained on the command callback with `.Then`, as the
  //   command callback now returns a value.
  // Note: The `on_complete` callback will be called if the system has already
  // been shut down.
  template <typename LockType,
            typename DescriptionType = typename LockType::LockDescription>
  void ScheduleCallbackWithLock(
      const std::string& operation_name,
      std::unique_ptr<DescriptionType> lock_description,
      base::OnceCallback<base::Value(LockType& lock)> callback,
      const base::Location& location = FROM_HERE,
      base::OnceClosure on_complete = base::DoNothing());

  // Schedules to clear the browsing data for web app, given the inclusive time
  // range.
  void ClearWebAppBrowsingData(const base::Time& begin_time,
                               const base::Time& end_time,
                               base::OnceClosure done,
                               const base::Location& location = FROM_HERE);

  // Launches the given app. This call also uses keep-alives to guarantee that
  // the browser and profile will not destruct before the launch is complete.
  void LaunchApp(const webapps::AppId& app_id,
                 const base::CommandLine& command_line,
                 const base::FilePath& current_directory,
                 const absl::optional<GURL>& url_handler_launch_url,
                 const absl::optional<GURL>& protocol_handler_launch_url,
                 const absl::optional<GURL>& file_launch_url,
                 const std::vector<base::FilePath>& launch_files,
                 LaunchWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Launches the given app to the given url, using keep-alives to guarantee the
  // browser and profile stay alive. Will CHECK-fail if `url` is not valid.
  void LaunchUrlInApp(const webapps::AppId& app_id,
                      const GURL& url,
                      LaunchWebAppCallback callback,
                      const base::Location& location = FROM_HERE);

  // Used to launch apps with a custom launch params. This does not respect the
  // configuration of the app, and will respect whatever the params say.
  void LaunchAppWithCustomParams(apps::AppLaunchParams params,
                                 LaunchWebAppCallback callback,
                                 const base::Location& location = FROM_HERE);

  // Used to locally install an app from the chrome://apps page, triggered
  // by the AppLauncherHandler.
  void InstallAppLocally(const webapps::AppId& app_id,
                         base::OnceClosure callback,
                         const base::Location& location = FROM_HERE);

  // Used to schedule a synchronization of a web app's OS states with the
  // current DB states.
  void SynchronizeOsIntegration(
      const webapps::AppId& app_id,
      base::OnceClosure synchronize_callback,
      absl::optional<SynchronizeOsOptions> synchronize_options = absl::nullopt,
      const base::Location& location = FROM_HERE);

  // Finds web apps that share the same install URLs (possibly across different
  // install sources) and dedupes the install URL configs into the most
  // recently installed non-placeholder-like web app.
  // Placeholder-like web apps are either marked as placeholder or have
  // their name set to their start URL like a placeholder. This is an erroneous
  // state some web apps have gotten into, see https://crbug.com/1427340.
  void ScheduleDedupeInstallUrls(base::OnceClosure callback,
                                 const base::Location& location = FROM_HERE);

  // Sets the user preference for link capturing for the given app. If
  // `set_to_preferred` is true, then links in the browser can be launched in
  // the app corresponding to app_id, respecting the app's launch handler
  // preferences. Additionally, if there are multiple apps within the same
  // scope, this will reset the preference on those apps to false.
  void SetAppCapturesSupportedLinksDisableOverlapping(
      const webapps::AppId app_id,
      bool set_to_preferred,
      base::OnceClosure done,
      const base::Location& location = FROM_HERE);

  // TODO(https://crbug.com/1298130): expose all commands for web app
  // operations.

 private:
  void LaunchApp(apps::AppLaunchParams params,
                 LaunchWebAppWindowSetting option,
                 LaunchWebAppCallback callback,
                 const base::Location& location);

  void LaunchAppWithKeepAlives(
      apps::AppLaunchParams params,
      LaunchWebAppWindowSetting option,
      LaunchWebAppCallback callback,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
      const base::Location& location);

  bool IsShuttingDown() const;

  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool is_in_shutdown_ = false;

  // Track how many times ScheduleDedupeInstallUrls() is invoked for metrics to
  // check that it's not happening excessively.
  // TODO(crbug.com/1434692): Remove once validating that the numbers look okay
  // out in the wild.
  size_t dedupe_install_urls_run_count_ = 0;

  base::WeakPtrFactory<WebAppCommandScheduler> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
