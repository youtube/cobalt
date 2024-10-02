// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_

#include <stdint.h>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_cache.h"
#include "components/services/app_service/public/cpp/icon_coalescer.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

class Profile;
class GURL;

namespace base {
class FilePath;
}

namespace apps {

class AppPublisher;
class AppUpdate;
class BrowserAppLauncher;
class PreferredAppsListHandle;
struct AppLaunchParams;

struct IntentLaunchInfo {
  IntentLaunchInfo();
  ~IntentLaunchInfo();
  IntentLaunchInfo(const IntentLaunchInfo& other);

  std::string app_id;
  std::string activity_name;
  std::string activity_label;
  bool is_generic_file_handler;
  bool is_file_extension_match;
  // Whether the intent is blocked by DLP. Defaults to false.
  bool is_dlp_blocked = false;
};

// Singleton (per Profile) proxy and cache of an App Service's apps.
//
// Singleton-ness means that //chrome/browser code (e.g UI code) can find *the*
// proxy for a given Profile, and therefore share its caches.
// Observe AppRegistryCache to delete the preferred app on app removed.
//
// On all platforms, there is no instance for incognito profiles.
// On Chrome OS, an instance is created for the guest session profile and the
// lock screen apps profile, but not for the signin profile.
//
// See components/services/app_service/README.md.
class AppServiceProxyBase : public KeyedService,
                            public IconLoader,
                            public PreferredAppsImpl::Host {
 public:
  explicit AppServiceProxyBase(Profile* profile);
  AppServiceProxyBase(const AppServiceProxyBase&) = delete;
  AppServiceProxyBase& operator=(const AppServiceProxyBase&) = delete;
  ~AppServiceProxyBase() override;

  void ReinitializeForTesting(
      Profile* profile,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());

  Profile* profile() const { return profile_; }

  apps::AppRegistryCache& AppRegistryCache();
  apps::AppCapabilityAccessCache& AppCapabilityAccessCache();

  apps::BrowserAppLauncher* BrowserAppLauncher();

  apps::PreferredAppsListHandle& PreferredAppsList();

  // Registers `publisher` with the App Service as exclusively publishing apps
  // of type `app_type`. `publisher` must have a lifetime equal to or longer
  // than this object.
  void RegisterPublisher(AppType app_type, AppPublisher* publisher);

  // UnRegisters the publisher for `app_type`, As the publisher(ArcApps) might
  // be destroyed earlier than AppServiceProxy.
  void UnregisterPublisher(AppType app_type);

  // PreferredAppsImpl::Host overrides.
  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override;

  // apps::IconLoader overrides.
  absl::optional<IconKey> GetIconKey(const std::string& app_id) override;
  std::unique_ptr<Releaser> LoadIconFromIconKey(
      AppType app_type,
      const std::string& app_id,
      const IconKey& icon_key,
      IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      LoadIconCallback callback) override;

  // Launches the app for the given |app_id|. |event_flags| provides additional
  // context about the action which launches the app (e.g. a middle click
  // indicating opening a background tab). |launch_source| is the possible app
  // launch sources, e.g. from Shelf, from the search box, etc. |window_info| is
  // the window information to launch an app, e.g. display_id, window bounds.
  //
  // Note: prefer using LaunchSystemWebAppAsync() for launching System Web Apps,
  // as that is robust to the choice of profile and avoids needing to specify an
  // app_id.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info = nullptr);

  // Launches the app for the given |app_id| with files from |file_paths|.
  // DEPRECATED. Prefer passing the files in an Intent through
  // LaunchAppWithIntent.
  // TODO(crbug.com/1264164): Remove this method.
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths);

  // Launches an app for the given |app_id|, passing |intent| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  virtual void LaunchAppWithIntent(const std::string& app_id,
                                   int32_t event_flags,
                                   IntentPtr intent,
                                   LaunchSource launch_source,
                                   WindowInfoPtr window_info,
                                   LaunchCallback callback);

  // Launches an app for the given |app_id|, passing |url| to the app.
  // |event_flags| provides additional context about the action which launch the
  // app (e.g. a middle click indicating opening a background tab).
  // |launch_source| is the possible app launch sources. |window_info| is the
  // window information to launch an app, e.g. display_id, window bounds.
  void LaunchAppWithUrl(const std::string& app_id,
                        int32_t event_flags,
                        GURL url,
                        LaunchSource launch_source,
                        WindowInfoPtr window_info = nullptr,
                        LaunchCallback callback = base::DoNothing());

  // Launches an app for the given |params.app_id|. The |params| can also
  // contain other param such as launch container, window diposition, etc.
  // Currently the return value in the callback will only be filled up for
  // Chrome OS web apps and Chrome apps.
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback = base::DoNothing());

  // Sets |permission| for the app identified by |app_id|.
  void SetPermission(const std::string& app_id, PermissionPtr permission);

  // Uninstalls an app for the given |app_id|. If |parent_window| is specified,
  // the uninstall dialog will be created as a modal dialog anchored at
  // |parent_window|. Otherwise, the browser window will be used as the anchor.
  virtual void Uninstall(const std::string& app_id,
                         UninstallSource uninstall_source,
                         gfx::NativeWindow parent_window) = 0;

  // Uninstalls an app for the given |app_id| without prompting the user to
  // confirm.
  void UninstallSilently(const std::string& app_id,
                         UninstallSource uninstall_source);

  // Stops the current running app for the given |app_id|.
  void StopApp(const std::string& app_id);

  // Returns the menu items for the given |app_id|. |display_id| is the id of
  // the display from which the app is launched.
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback);

  // Executes a shortcut menu |command_id| and |shortcut_id| for a menu item
  // previously built with GetMenuModel(). |app_id| is the menu app.
  // |display_id| is the id of the display from which the app is launched.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id);

  // Opens native settings for the app with |app_id|.
  void OpenNativeSettings(const std::string& app_id);

  apps::IconLoader* OverrideInnerIconLoaderForTesting(
      apps::IconLoader* icon_loader);

  // Returns a list of apps (represented by their ids) which can handle |url|.
  // If |exclude_browsers| is true, then exclude the browser apps.
  // If |exclude_browser_tab_apps| is true then exclude apps that open in
  // browser tabs.
  std::vector<std::string> GetAppIdsForUrl(
      const GURL& url,
      bool exclude_browsers = false,
      bool exclude_browser_tab_apps = true);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |intent|. If |exclude_browsers| is true, then
  // exclude the browser apps. If |exclude_browser_tab_apps| is true then
  // exclude apps that open in browser tabs.
  std::vector<IntentLaunchInfo> GetAppsForIntent(
      const apps::IntentPtr& intent,
      bool exclude_browsers = false,
      bool exclude_browser_tab_apps = true);

  // Returns a list of apps (represented by their ids) and activities (if
  // applied) which can handle |files|.
  std::vector<IntentLaunchInfo> GetAppsForFiles(
      std::vector<apps::IntentFilePtr> files);

  // Adds a preferred app for |url|.
  // Deprecated, prefer calling SetSupportedLinksPreference() instead.
  // TODO(crbug.com/1416434): Migrate existing users.
  void AddPreferredApp(const std::string& app_id, const GURL& url);
  // Adds a preferred app for |intent|. Only supports link intents.
  // Deprecated, prefer calling SetSupportedLinksPreference() instead.
  // TODO(crbug.com/1416434): Migrate existing users.
  void AddPreferredApp(const std::string& app_id, const IntentPtr& intent);

  // Sets |app_id| as the preferred app for all of its supported links ('view'
  // intent filters with a scheme and host). Any existing preferred apps for
  // those links will have all their supported links unset, as if
  // RemoveSupportedLinksPreference was called for that app.
  void SetSupportedLinksPreference(const std::string& app_id);

  // Set |app_id| as preferred app for all its supported link filters. Supported
  // link filters, which have the http/https scheme and at least one host, are
  // always enabled/disabled as a group. |all_link_filters| should contain all
  // of the apps' Supported Link intent filters.
  // Any apps with overlapping preferred app preferences will have all their
  // supported link filters unset, as if RemoveSupportedLinksPreference was
  // called for that app.
  // TODO(crbug.com/1265315): Remove this method to use
  // SetSupportedLinksPreference(std::string).
  void SetSupportedLinksPreference(const std::string& app_id,
                                   IntentFilters all_link_filters);

  // Removes all supported link filters from the preferred app list for
  // |app_id|.
  void RemoveSupportedLinksPreference(const std::string& app_id);

  // Sets the window display mode for the app identified by `app_id`.
  // `window_mode` represents how the app will be open in (e.g. in a
  // standalone window or in a browser tab).
  void SetWindowMode(const std::string& app_id, WindowMode window_mode);

  // Called by an app publisher to inform the proxy of a change in app state.
  virtual void OnApps(std::vector<AppPtr> deltas,
                      AppType app_type,
                      bool should_notify_initialized);

  // Called by an app publisher to inform the proxy of a change in
  // CapabilityAccess.
  void OnCapabilityAccesses(std::vector<CapabilityAccessPtr> deltas);

 protected:
  // An adapter, presenting an IconLoader interface based on the underlying
  // service (or on a fake implementation for testing).
  //
  // Conceptually, the ASP (the AppServiceProxyBase) is itself such an adapter:
  // UI clients call the IconLoader::LoadIconFromIconKey method (which the ASP
  // implements) and the ASP translates (i.e. adapts) these to publisher's
  // LoadIcon calls (or C++ calls to the Fake). This diagram shows control flow
  // going left to right (with "=c=>" and "=> Publisher::LoadIcon" denoting C++
  // and publisher's LoadIcon calls), and the responses (callbacks) then run
  // right to left in LIFO order:
  //
  //   UI =c=> ASP => Publisher::LoadIcon
  //                |       or
  //                +=c=> Fake
  //
  // It is more complicated in practice, as we want to insert IconLoader
  // decorators (as in the classic "decorator" or "wrapper" design pattern) to
  // provide optimizations like proxy-wide icon caching and IPC coalescing
  // (de-duplication). Nonetheless, from a UI client's point of view, we would
  // still like to present a simple API: that the ASP implements the IconLoader
  // interface. We therefore split the "ASP" component into multiple
  // sub-components. Once again, control flow runs from left to right, and
  // inside the ASP, outer layers (wrappers) call into inner layers (wrappees):
  //
  //           +------------------ ASP ------------------+
  //           |                                         |
  //   UI =c=> | Outer =c=> MoreDecorators... =c=> Inner | =>
  //   Publisher::LoadIcon
  //           |                                         |  |       or
  //           +-----------------------------------------+  +=c=> Fake
  //
  // The inner_icon_loader_ field (of type InnerIconLoader) is the "Inner"
  // component: the one that ultimately talks to the Mojo service.
  //
  // The outer_icon_loader_ field (of type IconCache) is the "Outer" component:
  // the entry point for calls into the AppServiceProxyBase.
  //
  // Note that even if the ASP provides some icon caching, upstream UI clients
  // may want to introduce further icon caching. See the commentary where
  // IconCache::GarbageCollectionPolicy is defined.
  //
  // IPC coalescing would be one of the "MoreDecorators".
  class InnerIconLoader : public apps::IconLoader {
   public:
    explicit InnerIconLoader(AppServiceProxyBase* host);

    // apps::IconLoader overrides.
    absl::optional<IconKey> GetIconKey(const std::string& app_id) override;
    std::unique_ptr<Releaser> LoadIconFromIconKey(
        AppType app_type,
        const std::string& app_id,
        const IconKey& icon_key,
        IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::LoadIconCallback callback) override;

    // |host_| owns |this|, as the InnerIconLoader is an AppServiceProxyBase
    // field.
    raw_ptr<AppServiceProxyBase> host_;

    raw_ptr<apps::IconLoader> overriding_icon_loader_for_testing_;
  };

  virtual bool IsValidProfile();

  // Called in AppServiceProxyFactory::BuildServiceInstanceFor immediately
  // following the creation of AppServiceProxy. Use this method to perform any
  // operation that depends on a fully instantiated AppServiceProxy (i.e. to
  // avoid calling other virtual methods in the AppServiceProxy constructor).
  virtual void Initialize();

  AppPublisher* GetPublisher(AppType app_type);

  // Returns true if the app cannot be launched and a launch prevention dialog
  // is shown to the user (e.g. the app is paused or blocked). Returns false
  // otherwise (and the app can be launched).
  virtual bool MaybeShowLaunchPreventionDialog(
      const apps::AppUpdate& update) = 0;

  IntentFilterPtr FindBestMatchingFilter(const IntentPtr& intent);

  virtual void PerformPostLaunchTasks(apps::LaunchSource launch_source);

  virtual void RecordAppPlatformMetrics(Profile* profile,
                                        const apps::AppUpdate& update,
                                        apps::LaunchSource launch_source,
                                        apps::LaunchContainer container);

  virtual void PerformPostUninstallTasks(apps::AppType app_type,
                                         const std::string& app_id,
                                         UninstallSource uninstall_source);

  virtual void OnLaunched(LaunchCallback callback,
                          LaunchResult&& launch_result);

  // Returns true if we should read icon image files from the local app_service
  // icon directory on disk, e.g. for ChromeOS. Otherwise, returns false.
  virtual bool ShouldReadIcons(AppType app_type);

  // Reads icon image files from the local app_service icon directory on disk.
  virtual void ReadIcons(AppType app_type,
                         const std::string& app_id,
                         int32_t size_in_dip,
                         std::unique_ptr<IconKey> icon_key,
                         IconType icon_type,
                         LoadIconCallback callback) {}

  // Returns an instance of `IntentLaunchInfo` created based on `intent`,
  // `filter`, and `update`.
  virtual IntentLaunchInfo CreateIntentLaunchInfo(
      const apps::IntentPtr& intent,
      const apps::IntentFilterPtr& filter,
      const apps::AppUpdate& update);

  base::flat_map<AppType, AppPublisher*> publishers_;

  apps::AppRegistryCache app_registry_cache_;
  apps::AppCapabilityAccessCache app_capability_access_cache_;

  // The LoadIconFromIconKey implementation sends a chained series of requests
  // through each icon loader, starting from the outer and working back to the
  // inner. Fields are listed from inner to outer, the opposite of call order,
  // as each one depends on the previous one, and in the constructor,
  // initialization happens in field order.
  InnerIconLoader inner_icon_loader_;
  IconCoalescer icon_coalescer_;
  IconCache outer_icon_loader_;

  std::unique_ptr<apps::PreferredAppsImpl> preferred_apps_impl_;

  raw_ptr<Profile> profile_;

  // TODO(crbug.com/1061843): Remove BrowserAppLauncher and merge the interfaces
  // to AppServiceProxyBase when publishers(ExtensionApps and WebApps) can run
  // on Chrome.
  std::unique_ptr<apps::BrowserAppLauncher> browser_app_launcher_;

  bool is_using_testing_profile_ = false;
  base::OnceClosure dialog_created_callback_;

 private:
  // For access to Initialize.
  friend class AppServiceProxyFactory;

  // For test access to OnApps.
  friend class AppServiceProxyPreferredAppsTest;

  base::WeakPtrFactory<AppServiceProxyBase> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_BASE_H_
