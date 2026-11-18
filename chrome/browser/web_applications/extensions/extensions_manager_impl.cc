// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/extensions_manager_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/preinstalled_extensions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_gate.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace web_app {

// This class registers itself to DelayedInstallManager on construction and
// unregisters itself on destruction. It always delays extension install.
class ExtensionInstallGateImpl : public extensions::InstallGate,
                                 public ExtensionInstallGate {
 public:
  explicit ExtensionInstallGateImpl(Profile* profile) : profile_(profile) {
    CHECK(profile);
    extensions::DelayedInstallManager::Get(profile)->RegisterInstallGate(
        extensions::ExtensionPrefs::DelayReason::kGc,
        static_cast<ExtensionInstallGateImpl*>(this));
  }

  ~ExtensionInstallGateImpl() override {
    extensions::DelayedInstallManager::Get(profile_)->UnregisterInstallGate(
        this);
  }

  extensions::InstallGate::Action ShouldDelay(
      const extensions::Extension* extension,
      bool install_immediately) override {
    return extensions::InstallGate::DELAY;
  }

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

ExtensionsManagerImpl::ExtensionsManagerImpl(Profile* profile)
    : profile_(profile),
      registry_(extensions::ExtensionRegistry::Get(profile)) {}

ExtensionsManagerImpl::~ExtensionsManagerImpl() = default;

void ExtensionsManagerImpl::OnExtensionSystemReady(base::OnceClosure closure) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(FROM_HERE,
                                                           std::move(closure));
}

std::unordered_set<base::FilePath>
ExtensionsManagerImpl::GetIsolatedStoragePaths() {
  std::unordered_set<base::FilePath> allowlist;
  extensions::ExtensionSet extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& ext : extensions) {
    if (extensions::util::HasIsolatedStorage(*ext.get(), profile_)) {
      allowlist.insert(extensions::util::GetStoragePartitionForExtensionId(
                           ext->id(), profile_)
                           ->GetPath());
    }
  }
  return allowlist;
}

std::unique_ptr<ExtensionInstallGate>
ExtensionsManagerImpl::RegisterGarbageCollectionInstallGate() {
  return std::make_unique<web_app::ExtensionInstallGateImpl>(profile_);
}

bool ExtensionsManagerImpl::IsExtensionBlockedByPolicy(
    const std::string& extension_id) {
  if (!registry_) {
    return false;
  }
  const extensions::Extension* extension =
      registry_->GetInstalledExtension(extension_id);
  extensions::ExtensionManagement* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  extensions::ManagedInstallationMode mode =
      extension ? management->GetInstallationMode(extension)
                : management->GetInstallationMode(extension_id,
                                                  /*update_url=*/std::string());
  return mode == extensions::ManagedInstallationMode::kBlocked ||
         mode == extensions::ManagedInstallationMode::kRemoved;
}

bool ExtensionsManagerImpl::IsExtensionInstalled(
    const std::string& extension_id) {
  return registry_ && registry_->GetInstalledExtension(extension_id);
}

bool ExtensionsManagerImpl::IsExtensionForceInstalled(
    const std::string& extension_id,
    std::u16string* reason) {
  return extensions::util::IsExtensionForceInstalled(extension_id, profile_,
                                                     reason);
}

bool ExtensionsManagerImpl::IsExtensionDefaultInstalled(
    const std::string& extension_id) {
  if (!registry_) {
    return false;
  }
  const extensions::Extension* extension =
      registry_->GetInstalledExtension(extension_id);
  return extension && (extension->creation_flags() &
                       extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
}

bool ExtensionsManagerImpl::IsExternalExtensionUninstalled(
    const std::string& extension_id) {
  auto* prefs = extensions::ExtensionPrefs::Get(profile_);
  return prefs && prefs->IsExternalExtensionUninstalled(extension_id);
}

bool ExtensionsManagerImpl::DidPreinstalledAppsPerformNewInstallation() {
#if !BUILDFLAG(IS_CHROMEOS)
  return preinstalled_extensions::Provider::DidPerformNewInstallationForProfile(
      profile_);
#else
  return false;
#endif
}

bool ExtensionsManagerImpl::IsPreinstalledExtensionAppId(
    const std::string& app_id) {
  return extensions::chrome_app_deprecation::IsPreinstalledAppId(app_id);
}

void ExtensionsManagerImpl::CopyAppSortingLayout(
    const std::string& from_extension_id,
    const std::string& to_web_app_id) {
  extensions::AppSorting* app_sorting =
      extensions::ExtensionSystem::Get(profile_)->app_sorting();
  app_sorting->SetAppLaunchOrdinal(
      to_web_app_id, app_sorting->GetAppLaunchOrdinal(from_extension_id));
  app_sorting->SetPageOrdinal(to_web_app_id,
                              app_sorting->GetPageOrdinal(from_extension_id));
}

mojom::UserDisplayMode ExtensionsManagerImpl::GetExtensionUserDisplayMode(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      registry_->GetInstalledExtension(extension_id);
  CHECK(extension);

  if (extension->is_platform_app()) {
    return mojom::UserDisplayMode::kStandalone;
  }

  switch (extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile_), extension)) {
    case apps::LaunchContainer::kLaunchContainerWindow:
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      return mojom::UserDisplayMode::kStandalone;
    case apps::LaunchContainer::kLaunchContainerTab:
    case apps::LaunchContainer::kLaunchContainerNone:
      return mojom::UserDisplayMode::kBrowser;
  }
}

std::unique_ptr<ShortcutInfo> ExtensionsManagerImpl::GetExtensionShortcutInfo(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      registry_->GetInstalledExtension(extension_id);
  CHECK(extension);
  return web_app::ShortcutInfoForExtensionAndProfile(extension, profile_);
}

void ExtensionsManagerImpl::WaitForExtensionShortcutsDeleted(
    const std::string& extension_id,
    base::OnceClosure callback) {
  web_app::WaitForExtensionShortcutsDeleted(extension_id, std::move(callback));
}

// Implementation of ExtensionsManager static methods:
// static
std::unique_ptr<ExtensionsManager> ExtensionsManager::CreateForProfile(
    Profile* profile) {
  return std::make_unique<ExtensionsManagerImpl>(profile);
}
// static
KeyedServiceBaseFactory* ExtensionsManager::GetExtensionSystemSharedFactory() {
  return extensions::ChromeExtensionSystemSharedFactory::GetInstance();
}

}  // namespace web_app
