// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include <utility>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/web_file_handlers/intent_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/app_display_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

namespace {

apps::InstallReason GetInstallReason(const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location()))
    return apps::InstallReason::kSystem;

  if (extensions::Manifest::IsPolicyLocation(extension->location()))
    return apps::InstallReason::kPolicy;

  if (extension->was_installed_by_oem())
    return apps::InstallReason::kOem;

  if (extension->was_installed_by_default())
    return apps::InstallReason::kDefault;

  return apps::InstallReason::kUser;
}

}  // namespace

// This class tracks all extension apps associated with a given Profile*. The
// observation of ExtensionPrefsObserver and ExtensionRegistryObserver is used
// to track AppService publisher events. The observation of AppsWindowRegistry
// is used to track window creation and destruction.
class LacrosExtensionAppsPublisher::ProfileTracker
    : public extensions::ExtensionPrefsObserver,
      public extensions::ExtensionRegistryObserver,
      public extensions::AppWindowRegistry::Observer {
  using Readiness = apps::Readiness;

 public:
  ProfileTracker(Profile* profile,
                 LacrosExtensionAppsPublisher* publisher,
                 const ForWhichExtensionType& which_type)
      : profile_(profile), publisher_(publisher), which_type_(which_type) {
    // Start observing for relevant events.
    prefs_observation_.Observe(extensions::ExtensionPrefs::Get(profile_));
    registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile_));
    app_window_registry_observation_.Observe(
        extensions::AppWindowRegistry::Get(profile_));

    // Populate initial conditions [e.g. installed apps prior to starting
    // observation].
    std::vector<apps::AppPtr> apps;
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->enabled_extensions()) {
      if (which_type_.Matches(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kReady));
      }
    }
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->disabled_extensions()) {
      if (which_type_.Matches(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kDisabledByUser));
      }
    }
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->terminated_extensions()) {
      if (which_type_.Matches(extension.get())) {
        apps.push_back(MakeApp(extension.get(), Readiness::kTerminated));
      }
    }
    if (!apps.empty())
      Publish(std::move(apps));

    if (which_type_.IsChromeApps()) {
      // Populate initial conditions [e.g. app windows created prior to starting
      // observation].
      for (extensions::AppWindow* app_window :
           extensions::AppWindowRegistry::Get(profile_)->app_windows()) {
        OnAppWindowAdded(app_window);
      }
    }
  }

  void Publish(const extensions::Extension* extension, Readiness readiness) {
    Publish(MakeApp(extension, readiness));
  }

  ~ProfileTracker() override = default;

 private:
  // extensions::ExtensionPrefsObserver overrides.
  void OnExtensionLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override {
    const auto* extension =
        lacros_extensions_util::MaybeGetExtension(profile_, app_id);
    if (!extension || !which_type_.Matches(extension))
      return;

    Publish(MakeApp(extension, Readiness::kReady));
  }

  void OnExtensionPrefsWillBeDestroyed(
      extensions::ExtensionPrefs* prefs) override {
    DCHECK(prefs_observation_.IsObservingSource(prefs));
    prefs_observation_.Reset();
  }

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    if (!which_type_.Matches(extension))
      return;
    Publish(MakeApp(extension, Readiness::kReady));
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (!which_type_.Matches(extension))
      return;

    Readiness readiness = Readiness::kUnknown;

    switch (reason) {
      case extensions::UnloadedExtensionReason::DISABLE:
        readiness = Readiness::kDisabledByUser;
        break;
      case extensions::UnloadedExtensionReason::BLOCKLIST:
        readiness = Readiness::kDisabledByBlocklist;
        break;
      case extensions::UnloadedExtensionReason::TERMINATE:
        readiness = Readiness::kTerminated;
        break;
      case extensions::UnloadedExtensionReason::UNINSTALL:
        // App readiness will be updated by OnExtensionUninstalled(). We defer
        // to that method.
        return;
      case extensions::UnloadedExtensionReason::UNDEFINED:
      case extensions::UnloadedExtensionReason::UPDATE:
      case extensions::UnloadedExtensionReason::PROFILE_SHUTDOWN:
      case extensions::UnloadedExtensionReason::LOCK_ALL:
      case extensions::UnloadedExtensionReason::MIGRATED_TO_COMPONENT:
        return;
    }
    Publish(MakeApp(extension, readiness));
  }

  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override {
    if (!which_type_.Matches(extension))
      return;
    Publish(MakeApp(extension, Readiness::kReady));
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    if (!which_type_.Matches(extension))
      return;
    apps::AppPtr app =
        MakeApp(extension, reason == extensions::UNINSTALL_REASON_MIGRATED
                               ? Readiness::kUninstalledByNonUser
                               : Readiness::kUninstalledByUser);
    Publish(std::move(app));
  }

  void OnShutdown(extensions::ExtensionRegistry* registry) override {
    registry_observation_.Reset();
  }

  // AppWindowRegistry::Observer overrides.
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    // Only chrome app windows are added to the dock.
    if (!which_type_.IsChromeApps())
      return;
    // The extension also has to match.
    if (!which_type_.Matches(app_window->GetExtension()))
      return;
    std::string window_id = lacros_window_utility::GetRootWindowUniqueId(
        app_window->GetNativeWindow());
    app_window_id_cache_[app_window] = window_id;

    publisher_->OnAppWindowAdded(app_window->GetExtension()->id(), window_id);
  }

  void OnAppWindowRemoved(extensions::AppWindow* app_window) override {
    // Only chrome app windows are added to the dock.
    if (!which_type_.IsChromeApps())
      return;
    // The extension also has to match. As the extension may be destroyed at
    // this point, we use presence in app_window_id_cache_ to decide whether to
    // continue.
    auto it = app_window_id_cache_.find(app_window);
    if (it == app_window_id_cache_.end())
      return;

    std::string window_id = it->second;
    publisher_->OnAppWindowRemoved(app_window->extension_id(), window_id);

    app_window_id_cache_.erase(app_window);
  }

  // Publishes a differential update to the app service.
  void Publish(apps::AppPtr app) {
    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));
    Publish(std::move(apps));
  }

  // Publishes a vector of differential updates to the app service.
  void Publish(std::vector<apps::AppPtr> apps) {
    publisher_->Publish(std::move(apps));
  }

  // Whether the app should be shown in the launcher, shelf, etc.
  bool ShouldShow(const extensions::Extension* extension) {
    if (which_type_.IsExtensions())
      return false;
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    const std::string& app_id = extension->id();
    // These three extension sets are the same three consulted by the
    // constructor. Importantly, it will exclude previously installed but
    // currently uninstalled extensions.
    bool connected = registry->enabled_extensions().Contains(app_id) ||
                     registry->disabled_extensions().Contains(app_id) ||
                     registry->terminated_extensions().Contains(app_id);
    if (!connected)
      return false;

    return extensions::ui_util::ShouldDisplayInAppLauncher(extension, profile_);
  }

  // Creates an AppPtr from an extension.
  apps::AppPtr MakeApp(const extensions::Extension* extension,
                       Readiness readiness) {
    DCHECK(which_type_.Matches(extension));
    apps::AppType app_type = which_type_.ChooseForChromeAppOrExtension(
        apps::AppType::kStandaloneBrowserChromeApp,
        apps::AppType::kStandaloneBrowserExtension);
    auto app = std::make_unique<apps::App>(app_type, extension->id());
    app->readiness = readiness;
    app->name = extension->name();
    app->short_name = extension->short_name();

    // TODO(crbug.com/1367337): Work out how pinning interacts with Lacros
    // multi-profile support once there is a product decision on what that looks
    // like.
    app->policy_ids = {extension->id()};

    // We always use an empty icon key since we currently do not support
    // dynamically changing icons or modifying the appearance of icons.
    // This bug is tracked at https://crbug.com/1248499, but given that Chrome
    // Apps is deprecated, it's unclear if we'll ever get around to implementing
    // this functionality.
    app->icon_key =
        apps::IconKey(/*timeline=*/0, apps::IconKey::kInvalidResourceId,
                      apps::IconEffects::kCrOsStandardIcon);

    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      app->install_time = prefs->GetLastUpdateTime(extension->id());
    } else {
      app->last_launch_time = base::Time();
      app->install_time = base::Time();
    }

    app->install_reason = GetInstallReason(extension);
    app->recommendable = true;
    app->searchable = true;
    app->paused = false;

    bool show = ShouldShow(extension);
    app->show_in_launcher = show;
    app->show_in_shelf = show;
    app->show_in_search = show;
    app->show_in_management =
        extensions::AppDisplayInfo::ShouldDisplayInAppLauncher(*extension);
    app->handles_intents = which_type_.IsExtensions() || show;

    if (which_type_.IsChromeApps()) {
      app->is_platform_app = extension->is_platform_app();

      if (extension->is_hosted_app()) {
        app->window_mode =
            extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile_),
                                      extension) ==
                    extensions::LaunchType::LAUNCH_TYPE_WINDOW
                ? apps::WindowMode::kWindow
                : apps::WindowMode::kBrowser;
      }
    }

    const extensions::ManagementPolicy* policy =
        extensions::ExtensionSystem::Get(profile_)->management_policy();
    app->allow_uninstall = (policy->UserMayModifySettings(extension, nullptr) &&
                            !policy->MustRemainInstalled(extension, nullptr));

    // Add file_handlers for either of the following:
    //   a) Chrome Apps and quickoffice.
    //   b) Web File Handlers or file_browser_handler for Extensions.
    base::Extend(app->intent_filters,
                 which_type_.ChooseIntentFilter(
                     extensions::IsLegacyQuickOfficeExtension(*extension),
                     apps_util::CreateIntentFiltersForChromeApp,
                     apps_util::CreateIntentFiltersForExtension)(extension));
    return app;
  }

  // This pointer is guaranteed to be valid and to outlive this object.
  const raw_ptr<Profile> profile_;

  // This pointer is guaranteed to be valid and to outlive this object.
  const raw_ptr<LacrosExtensionAppsPublisher> publisher_;

  // State to decide which extension type (e.g., Chrome Apps vs. Extensions)
  // to support.
  const ForWhichExtensionType which_type_;

  // Observes both extension prefs and registry for events that affect
  // extensions.
  base::ScopedObservation<extensions::ExtensionPrefs,
                          extensions::ExtensionPrefsObserver>
      prefs_observation_{this};
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};

  // Observes AppWindowRegistry for app window creation and destruction.
  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_observation_{this};

  // Records the window id associated with an app window. This is needed since
  // the app window destruction callback occurs after the window is destroyed.
  std::map<extensions::AppWindow*, std::string> app_window_id_cache_;
};

// static
std::unique_ptr<LacrosExtensionAppsPublisher>
LacrosExtensionAppsPublisher::MakeForChromeApps() {
  return std::make_unique<LacrosExtensionAppsPublisher>(InitForChromeApps());
}

// static
std::unique_ptr<LacrosExtensionAppsPublisher>
LacrosExtensionAppsPublisher::MakeForExtensions() {
  return std::make_unique<LacrosExtensionAppsPublisher>(InitForExtensions());
}

LacrosExtensionAppsPublisher::LacrosExtensionAppsPublisher(
    const ForWhichExtensionType& which_type)
    : which_type_(which_type) {}

LacrosExtensionAppsPublisher::~LacrosExtensionAppsPublisher() = default;

void LacrosExtensionAppsPublisher::Initialize() {
  if (!InitializeCrosapi())
    return;
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    // TODO(https://crbug.com/1254894): The app id is not stable for secondary
    // profiles and cannot be stored in sync. Thus, the app cannot be published
    // at all.
    if (!profile->IsMainProfile())
      continue;
    profile_trackers_[profile] =
        std::make_unique<ProfileTracker>(profile, this, which_type_);
  }

  // Only track the media usage for the chrome apps.
  if (which_type_.IsChromeApps()) {
    media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                                  ->GetMediaStreamCaptureIndicator()
                                  .get());
  }
}

bool LacrosExtensionAppsPublisher::InitializeCrosapi() {
  // Ash is too old to support the chrome app publisher interface.
  int crosapiVersion = chromeos::LacrosService::Get()
                           ->GetInterfaceVersion<crosapi::mojom::Crosapi>();
  int minRequiredVersion =
      static_cast<int>(which_type_.ChooseForChromeAppOrExtension(
          crosapi::mojom::Crosapi::kBindChromeAppPublisherMinVersion,
          crosapi::mojom::Crosapi::kBindExtensionPublisherMinVersion));
  if (crosapiVersion < minRequiredVersion)
    return false;

  // Ash is too old to support the chrome app window tracker interface.
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::AppWindowTracker>()) {
    return false;
  }

  if (which_type_.IsChromeApps()) {
    chromeos::LacrosService::Get()
        ->BindPendingReceiverOrRemote<
            mojo::PendingReceiver<crosapi::mojom::AppPublisher>,
            &crosapi::mojom::Crosapi::BindChromeAppPublisher>(
            publisher_.BindNewPipeAndPassReceiver());
  } else if (which_type_.IsExtensions()) {
    chromeos::LacrosService::Get()
        ->BindPendingReceiverOrRemote<
            mojo::PendingReceiver<crosapi::mojom::AppPublisher>,
            &crosapi::mojom::Crosapi::BindExtensionPublisher>(
            publisher_.BindNewPipeAndPassReceiver());
  }
  return true;
}

void LacrosExtensionAppsPublisher::Publish(std::vector<apps::AppPtr> apps) {
  publisher_->OnApps(std::move(apps));
}

void LacrosExtensionAppsPublisher::PublishCapabilityAccesses(
    std::vector<apps::CapabilityAccessPtr> accesses) {
  publisher_->OnCapabilityAccesses(std::move(accesses));
}

void LacrosExtensionAppsPublisher::OnAppWindowAdded(
    const std::string& app_id,
    const std::string& window_id) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppWindowTracker>()
      ->OnAppWindowAdded(app_id, window_id);
}

void LacrosExtensionAppsPublisher::OnAppWindowRemoved(
    const std::string& app_id,
    const std::string& window_id) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppWindowTracker>()
      ->OnAppWindowRemoved(app_id, window_id);
}

void LacrosExtensionAppsPublisher::OnProfileAdded(Profile* profile) {
  // TODO(https://crbug.com/1254894): The app id is not stable for secondary
  // profiles and cannot be stored in sync. Thus, the app cannot be published
  // at all.
  if (!profile->IsMainProfile())
    return;
  profile_trackers_[profile] =
      std::make_unique<ProfileTracker>(profile, this, which_type_);
}

void LacrosExtensionAppsPublisher::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  profile_trackers_.erase(profile);
}

void LacrosExtensionAppsPublisher::OnProfileManagerDestroying() {
  profile_trackers_.clear();
  profile_manager_observation_.Reset();
}

void LacrosExtensionAppsPublisher::UpdateAppWindowMode(
    const std::string& app_id,
    apps::WindowMode window_mode) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      app_id, &profile, &extension);
  if (!success)
    return;

  DCHECK(extension->is_hosted_app());

  // Persist hosted app's launch preference.
  extensions::SetLaunchType(profile, extension->id(),
                            window_mode == apps::WindowMode::kWindow
                                ? extensions::LAUNCH_TYPE_WINDOW
                                : extensions::LAUNCH_TYPE_REGULAR);

  // Republish the app.
  auto matched = profile_trackers_.find(profile);
  DCHECK(matched != profile_trackers_.end());
  matched->second->Publish(extension, apps::Readiness::kReady);
}

void LacrosExtensionAppsPublisher::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  auto app_id = MaybeGetAppId(web_contents);
  if (!app_id.has_value()) {
    return;
  }

  auto result = media_requests_.UpdateCameraState(app_id.value(), web_contents,
                                                  is_capturing_video);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

void LacrosExtensionAppsPublisher::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  auto app_id = MaybeGetAppId(web_contents);
  if (!app_id.has_value()) {
    return;
  }

  auto result = media_requests_.UpdateMicrophoneState(
      app_id.value(), web_contents, is_capturing_audio);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

absl::optional<std::string> LacrosExtensionAppsPublisher::MaybeGetAppId(
    content::WebContents* web_contents) {
  // The web app publisher is responsible to handle `web_contents` for web
  // apps.
  const webapps::AppId* web_app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (web_app_id) {
    return absl::nullopt;
  }

  const auto* extension =
      lacros_extensions_util::MaybeGetExtension(web_contents);
  return (extension && which_type_.Matches(extension))
             ? absl::make_optional<std::string>(extension->id())
             : absl::nullopt;
}

void LacrosExtensionAppsPublisher::ModifyCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::CapabilityAccessPtr> capability_accesses;
  auto capability_access = std::make_unique<apps::CapabilityAccess>(app_id);
  capability_access->camera = accessing_camera;
  capability_access->microphone = accessing_microphone;
  capability_accesses.push_back(std::move(capability_access));

  PublishCapabilityAccesses(std::move(capability_accesses));
}
