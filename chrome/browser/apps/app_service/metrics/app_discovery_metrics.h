// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// Represents the different state changes of interest for app-discovery. Keep
// in-sync with definition in structured.xml.
enum class AppStateChange {
  kInactive = 0,
  kActive = 1,
  kClosed = 2,
};

// Records metrics related to app discovery and app usage. This should only be
// used in Chrome OS.
//
// No metrics should be recorded if app-sync is off.
class AppDiscoveryMetrics : public AppPlatformMetrics::Observer,
                            InstanceRegistry::Observer {
 public:
  AppDiscoveryMetrics(Profile* profile,
                      InstanceRegistry& instance_registry,
                      AppPlatformMetrics* app_platform_metrics);
  ~AppDiscoveryMetrics() override;

  // AppPlatformMetrics::Observer
  void OnAppInstalled(const std::string& app_id,
                      AppType app_type,
                      InstallSource app_install_source,
                      InstallReason app_install_reason,
                      InstallTime app_install_time) override;
  void OnAppLaunched(const std::string& app_id,
                     AppType app_type,
                     LaunchSource launch_source) override;
  void OnAppUninstalled(const std::string& app_id,
                        AppType app_type,
                        UninstallSource app_uninstall_source) override;
  void OnAppPlatformMetricsDestroyed() override;

  // InstanceRegistry::Observer
  void OnInstanceUpdate(const InstanceUpdate& instance_update) override;
  void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) override;

 private:
  // Returns whether app sync is enabled for |profile_|.
  bool IsAppSyncEnabled();

  // Returns true if there is an active instance of an app other than
  // |exclude_instance_id|. If |exclude_instance_id| is nullopt, then all
  // instances will be checked.
  bool IsAnyAppInstanceActive(const std::string& app_id,
                              absl::optional<base::UnguessableToken>
                                  exclude_instance_id = absl::nullopt);

  // Records app state metrics if there has been a change.
  void RecordAppState(const InstanceUpdate& instance_update);

  // Checks whether |instance_update| moves from an active app state to inactive
  // app state. If there are any other instances that are active other than
  // the instance specified in |instance_update|, then the app is still
  // considered to be active and will return false.
  //
  // If there was no previous state, the check for whether the previous state is
  // Active will be ignored.
  bool IsUpdateActiveToInactive(const InstanceUpdate& instance_update);

  // Checks whether |instance_update| moves from an inactive app state to active
  // app state. If there are any other instances that are active other than
  // the instance specified in |instance_update|, then the app is still
  // considered to be active and will return false.
  //
  // If there was no previous state, the check for whether the previous state is
  // Inactive will be ignored.
  bool IsUpdateInactiveToActive(const InstanceUpdate& instance_update);

  bool IsStateInactive(InstanceState instance_state);
  bool IsStateActive(InstanceState instance_state);

  void RecordAppActive(const InstanceUpdate& instance_update);
  void RecordAppInactive(const InstanceUpdate& instance_update);
  void RecordAppClosed(const InstanceUpdate& instance_update);

  // Returns the string identifier to be logged for the given |profile_| and
  // |hashed_app_id|.
  //
  // For most apps, this should return the same string used to generate the URL
  // string key for UKM. A lot of these URLs are hashed before being collected.
  // For more information, refer to //components/ukm/app_source_url_recorder.h.
  //
  // ARC app package names are collected unhashed since the app package names
  // are public information on the play store.
  std::string GetAppStringToRecord(const std::string& hashed_app_id,
                                   AppType app_type);

  // Profile for which apps discovery metrics are being recorded for.
  raw_ptr<Profile, ExperimentalAsh> profile_;

  // Instance of AppPlatformMetrics |this| is observing.
  raw_ptr<AppPlatformMetrics, ExperimentalAsh> app_platform_metrics_ = nullptr;

  // Map associating instance_ids to current state.
  std::map<base::UnguessableToken, InstanceState> instance_to_state_;

  // Map associating app_ids to instance_ids.
  std::map<std::string, std::set<base::UnguessableToken>>
      app_id_to_instance_ids_;

  base::ScopedObservation<InstanceRegistry, InstanceRegistry::Observer>
      instance_registry_observation_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_
