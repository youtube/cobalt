// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keyed_service/core/keyed_service.h"

#if (BUILDFLAG(IS_CHROMEOS))
#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class Profile;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

class AbstractWebAppDatabaseFactory;
class WebAppSyncBridge;
class WebAppIconManager;
class PreinstalledWebAppManager;
class WebAppInstallFinalizer;
class ManifestUpdateManager;
class WebAppAudioFocusIdMap;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppUiManager;
class OsIntegrationManager;
class WebAppTranslationManager;
class WebAppCommandManager;
class WebAppCommandScheduler;
class WebAppOriginAssociationManager;

// WebAppProvider is the heart of Chrome web app code.
//
// Connects Web App features, such as the installation of default and
// policy-managed web apps, with Profiles (as WebAppProvider is a
// Profile-linked KeyedService) and their associated PrefService.
//
// Lifecycle notes:
// - WebAppProvider and its sub-managers are not ready for use until the
//   on_registry_ready() event has fired. Its database must be loaded from
//   disk before it can be interacted with.
//   Example of waiting for on_registry_ready():
//   WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
//   provider->on_registry_ready().Post(
//       FROM_HERE,
//       base::BindOnce([](WebAppProvider& provider) {
//         ...
//       }, std::ref(*provider));
// - All subsystems are constructed independently of each other in the
//   WebAppProvider constructor.
// - Subsystem construction should have no side effects and start no tasks.
// - Tests can replace any of the subsystems before Start() is called.
// - Similarly, in destruction, subsystems should not refer to each other.
class WebAppProvider : public KeyedService {
 public:
  // Deprecated: Use GetForWebApps instead.
  static WebAppProvider* GetDeprecated(Profile* profile);

  // On Chrome OS: if Lacros Web App (WebAppsCrosapi) is enabled, returns
  // WebAppProvider in Lacros and nullptr in Ash. Otherwise does the reverse
  // (nullptr in Lacros, WebAppProvider in Ash). On other platforms, always
  // returns a WebAppProvider.
  static WebAppProvider* GetForWebApps(Profile* profile);

  // Returns the WebAppProvider for the current process. In particular:
  // In Ash: Returns the WebAppProvider that hosts System Web Apps.
  // In Lacros and other platforms: Returns the WebAppProvider that hosts
  // non-system Web Apps.
  //
  // Avoid using this function where possible and prefer GetForWebApps which
  // provides a guarantee they are being called from the correct process. Only
  // use this if the calling code is shared between Ash and Lacros and expects
  // the PWA WebAppProvider in Lacros and the SWA WebAppProvider in Ash.
  static WebAppProvider* GetForLocalAppsUnchecked(Profile* profile);

  // Return the WebAppProvider for tests, regardless of whether this is running
  // in Lacros/Ash. Blocks if the web app registry is not yet ready.
  static WebAppProvider* GetForTest(Profile* profile);

  static WebAppProvider* GetForWebContents(content::WebContents* web_contents);

  using OsIntegrationManagerFactory =
      std::unique_ptr<OsIntegrationManager> (*)(Profile*);
  static void SetOsIntegrationManagerFactoryForTesting(
      OsIntegrationManagerFactory factory);

  explicit WebAppProvider(Profile* profile);
  WebAppProvider(const WebAppProvider&) = delete;
  WebAppProvider& operator=(const WebAppProvider&) = delete;
  ~WebAppProvider() override;

  // Start the Web App system. This will run subsystem startup tasks.
  void Start();

  // Read/write to web app system should use `scheduler()` to guarantee safe
  // access. This is safe to access even if the `WebAppProvider` is not ready.
  WebAppCommandScheduler& scheduler();
  //  This is safe to access even if the `WebAppProvider` is not ready.
  WebAppCommandManager& command_manager();

  // Web App sub components. These should only be accessed after
  // `on_registry_ready()` is signaled.

  // Unsafe access to the app registry model. For safe access use locks (see
  // chrome/browser/web_applications/locks/ for more info).
  WebAppRegistrar& registrar_unsafe();
  const WebAppRegistrar& registrar_unsafe() const;
  // Unsafe access to the WebAppSyncBridge. Reading or data from here should be
  // considered an 'uncommitted read', and writing data is unsafe and could
  // interfere with other operations. For safe access use locks to ensure no
  // operations (like install/update/uninstall/etc) are currently running. See
  // chrome/browser/web_applications/locks/ for more info.
  WebAppSyncBridge& sync_bridge_unsafe();
  // UIs can use WebAppInstallManager for user-initiated Web Apps install.
  WebAppInstallManager& install_manager();
  // Implements persistence for Web Apps install.
  WebAppInstallFinalizer& install_finalizer();
  // Keeps app metadata up to date with site manifests.
  ManifestUpdateManager& manifest_update_manager();
  // Clients can use ExternallyManagedAppManager to install, uninstall, and
  // update Web Apps.
  ExternallyManagedAppManager& externally_managed_app_manager();
  // Clients can use WebAppPolicyManager to request updates of policy installed
  // Web Apps.
  WebAppPolicyManager& policy_manager();

  WebAppUiManager& ui_manager();

  WebAppAudioFocusIdMap& audio_focus_id_map();

  // Implements fetching of app icons.
  WebAppIconManager& icon_manager();

  WebAppTranslationManager& translation_manager();

  // Manage all OS hooks that need to be deployed during Web Apps install
  OsIntegrationManager& os_integration_manager();
  const OsIntegrationManager& os_integration_manager() const;

  WebAppOriginAssociationManager& origin_association_manager();

  // KeyedService:
  void Shutdown() override;

  // Signals when app registry becomes ready.
  const base::OneShotEvent& on_registry_ready() const {
    return on_registry_ready_;
  }

  // Signals when external app managers have finished calling
  // `SynchronizeInstalledApps`, which means that all installs or uninstalls for
  // external managers have been scheduled. Specifically these calls are
  // triggered from the PreinstalledWebAppManager and the WebAppPolicyManager.
  // Note: This does not include the call from the ChromeOS SystemWebAppManager,
  // which is a separate keyed service.
  const base::OneShotEvent& on_external_managers_synchronized() const {
    return on_external_managers_synchronized_;
  }

  // Returns whether the app registry is ready.
  bool is_registry_ready() const { return is_registry_ready_; }

  PreinstalledWebAppManager& preinstalled_web_app_manager() {
    return *preinstalled_web_app_manager_;
  }

 protected:
  virtual void StartImpl();

  void CreateSubsystems(Profile* profile);

  // Wire together subsystems but do not start them (yet).
  void ConnectSubsystems();

  // Start sync bridge. All other subsystems depend on it.
  void StartSyncBridge();
  void OnSyncBridgeReady();

  void CheckIsConnected() const;

  // Performs a migration of some entries from the `web_app_ids` pref
  // dictionary to the web app database. This should be safe to delete one year
  // after 02-2022.
  void DoMigrateProfilePrefs(Profile* profile);

  std::unique_ptr<AbstractWebAppDatabaseFactory> database_factory_;
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<WebAppSyncBridge> sync_bridge_;
  std::unique_ptr<PreinstalledWebAppManager> preinstalled_web_app_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppTranslationManager> translation_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<ManifestUpdateManager> manifest_update_manager_;
  std::unique_ptr<ExternallyManagedAppManager> externally_managed_app_manager_;
  std::unique_ptr<WebAppAudioFocusIdMap> audio_focus_id_map_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
#if (BUILDFLAG(IS_CHROMEOS))
  std::unique_ptr<WebAppRunOnOsLoginManager> web_app_run_on_os_login_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<OsIntegrationManager> os_integration_manager_;
  std::unique_ptr<WebAppCommandManager> command_manager_;
  std::unique_ptr<WebAppCommandScheduler> command_scheduler_;
  std::unique_ptr<WebAppOriginAssociationManager> origin_association_manager_;

  base::OneShotEvent on_registry_ready_;
  base::OneShotEvent on_external_managers_synchronized_;

  const raw_ptr<Profile> profile_;

  // Ensures that ConnectSubsystems() is not called after Start().
  bool started_ = false;
  bool connected_ = false;
  bool is_registry_ready_ = false;

  base::WeakPtrFactory<WebAppProvider> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROVIDER_H_
