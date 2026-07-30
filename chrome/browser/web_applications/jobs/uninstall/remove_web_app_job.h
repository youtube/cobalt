// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"

class Profile;
class GURL;

namespace web_app {

class RemoveInstallSourceJob;

// This should VERY rarely be used directly, and instead just used from other
// jobs once all install managements are removed.
//
// NOTE: All removal operations must be implemented in both the job body
// (e.g. `SynchronizeAndUninstallOsHooks`, etc.) AND
// in the static `RemoveForCorruptDatabase` method. This is because
// `RemoveForCorruptDatabase` must be able to perform a "headless" cleanup of
// OS integration, icons, and other assets using only the salvaged `app_id`s
// when the database is too corrupted to load the full `WebApp` objects.
class RemoveWebAppJob : public UninstallJob {
 public:
  // Uninstalls all web apps from the database, deletes the database on disk,
  // and queues up tasks to delete all OS integration and storage partitions.
  // Note: This relies on `provider.database_factory()` to get the correct
  // DataTypeStore (either global or test fake) and will set
  // `prefs::kShouldGarbageCollectStoragePartitions` which schedules a command
  // that waits for the Extension System to be ready.
  //
  // A lock is not needed here because the entries in `salvaged_apps` are
  // corrupted apps that do not show up on the WebAppRegistrar. While we are
  // performing OS integration cleanup on them, they cannot conflict with
  // normal web app operations because they are not known to the rest of the
  // system.
  static void RemoveForCorruptDatabase(
      WebAppProvider& provider,
      const std::vector<std::pair<webapps::AppId, GURL>>& salvaged_apps,
      base::OnceClosure callback);

  // `webapps::IsUserUninstall(uninstall_source)` indicates that this operation
  // is not a byproduct of removing the last install source from a web app via
  // external management and will be treated as a user uninstall.
  RemoveWebAppJob(webapps::WebappUninstallSource uninstall_source,
                  Profile& profile,
                  base::DictValue& debug_value,
                  webapps::AppId app_id);
  ~RemoveWebAppJob() override;

  // UninstallJob:
  void Start(AllAppsLock& lock, Callback callback) override;
  webapps::WebappUninstallSource uninstall_source() const override;

 private:
  void SynchronizeAndUninstallOsHooks(base::OnceCallback<void(bool)> callback);
  void CheckOsIntegrationHooksRemoved(base::OnceCallback<void(bool)> callback);
  void RemoveOsIntegrationDirectory(base::OnceCallback<void(bool)> callback);
  void OnAllDataDeleted(std::vector<bool> results);
  void ProcessSubAppsPendingRemovalOrComplete();

  // Helper function to generate a callback that automatically records its
  // boolean success value into `debug_value_` under the provided `key` before
  // passing it along to the supplied `callback`.
  base::OnceCallback<void(bool)> GetLogCallback(
      std::string key,
      base::OnceCallback<void(bool)> callback);

  void CompleteAndSelfDestruct(webapps::UninstallResultCode code);

  const webapps::WebappUninstallSource uninstall_source_;
  // `this` must be owned by `profile_`.
  const raw_ref<Profile> profile_;
  const raw_ref<base::DictValue> debug_value_;
  const webapps::AppId app_id_;

  // `this` must be started and run within the scope of a WebAppCommand's
  // AllAppsLock.
  raw_ptr<AllAppsLock> lock_ = nullptr;
  Callback callback_;

  std::optional<webapps::UninstallResultCode> primary_removal_result_;

  std::vector<webapps::AppId> sub_apps_pending_removal_;
  std::unique_ptr<RemoveInstallSourceJob> sub_job_;

  base::WeakPtrFactory<RemoveWebAppJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_REMOVE_WEB_APP_JOB_H_
