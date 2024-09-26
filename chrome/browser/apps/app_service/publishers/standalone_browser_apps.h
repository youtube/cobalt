// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_APPS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/menu.h"

class Profile;

namespace apps {

class BrowserAppInstanceRegistry;
class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) for the "LaCrOS" app icon,
// which launches the lacros-chrome binary.
//
// See components/services/app_service/README.md.
class StandaloneBrowserApps : public AppPublisher,
                              public crosapi::BrowserManagerObserver {
 public:
  explicit StandaloneBrowserApps(AppServiceProxy* proxy);
  ~StandaloneBrowserApps() override;

  StandaloneBrowserApps(const StandaloneBrowserApps&) = delete;
  StandaloneBrowserApps& operator=(const StandaloneBrowserApps&) = delete;

 private:
  friend class PublisherHost;

  // Returns the single lacros app.
  AppPtr CreateStandaloneBrowserApp();

  void Initialize();

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;

  // crosapi::BrowserManagerObserver
  void OnLoadComplete(bool success, const base::Version& version) override;

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  bool is_browser_load_success_ = true;
  const raw_ptr<BrowserAppInstanceRegistry, DanglingUntriaged | ExperimentalAsh>
      browser_app_instance_registry_;
  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  // Used to observe the browser manager for image load changes.
  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      observation_{this};

  base::WeakPtrFactory<StandaloneBrowserApps> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_APPS_H_
