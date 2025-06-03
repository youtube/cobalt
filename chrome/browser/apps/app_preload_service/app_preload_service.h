// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class TimeTicks;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace apps {

class DeviceInfoManager;
class PreloadAppDefinition;
class WebAppPreloadInstaller;

struct DeviceInfo;

// Debugging feature to always run the App Preload Service on startup, even if
// the Profile would not normally be eligible.
BASE_DECLARE_FEATURE(kAppPreloadServiceForceRun);

// Debugging/testing feature to install test apps returned by the server, which
// are normally silently ignored.
BASE_DECLARE_FEATURE(kAppPreloadServiceEnableTestApps);

class AppPreloadService : public KeyedService {
 public:
  explicit AppPreloadService(Profile* profile);
  AppPreloadService(const AppPreloadService&) = delete;
  AppPreloadService& operator=(const AppPreloadService&) = delete;
  ~AppPreloadService() override;

  static AppPreloadService* Get(Profile* profile);

  // Registers prefs used for state management of the App Preload Service.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  using PreloadStatusCallback = base::OnceCallback<void(bool)>;

  // Starts the process of installing apps for first login, exposed for tests
  // to be able to control the timing of the flow. `callback` is called once
  // installation is complete with whether app installation was successful.
  void StartFirstLoginFlowForTesting(PreloadStatusCallback callback);

  // Disable the automatic preload flow which runs on AppPreloadService startup,
  // to allow tests to control the timing of preloads. Must be called before
  // AppPreloadService is created.
  static base::AutoReset<bool> DisablePreloadsOnStartupForTesting();

 private:
  // Starts the process of installing apps for first login. This method checks
  // eligibility and does not proceed with installation unless either the user
  // is new, or has previously failed to preload apps.
  void StartFirstLoginFlow();
  // This function begins the process to get a list of apps from the back end
  // service, processes the list and installs the app list. This call should
  // only be used the first time a profile is created on the device as this call
  // installs a set of default and OEM apps.
  void StartAppInstallationForFirstLogin(base::TimeTicks start_time,
                                         DeviceInfo device_info);
  // Processes the list of apps retrieved by the server connector.
  void OnGetAppsForFirstLoginCompleted(
      base::TimeTicks start_time,
      absl::optional<std::vector<PreloadAppDefinition>> apps);
  // Called when the installation flow started by
  // `StartAppInstallationForFirstLogin` is complete, with `success` indicating
  // whether the overall flow was successful.
  void OnFirstLoginFlowComplete(base::TimeTicks start_time, bool success);

  bool ShouldInstallApp(const PreloadAppDefinition& app);

  const base::Value::Dict& GetStateManager() const;

  raw_ptr<Profile> profile_;
  std::unique_ptr<AppPreloadServerConnector> server_connector_;
  std::unique_ptr<DeviceInfoManager> device_info_manager_;
  std::unique_ptr<WebAppPreloadInstaller> web_app_installer_;

  // For testing
  PreloadStatusCallback installation_complete_callback_;

  // `weak_ptr_factory_` must be the last member of this class.
  base::WeakPtrFactory<AppPreloadService> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
