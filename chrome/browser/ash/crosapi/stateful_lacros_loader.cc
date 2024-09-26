// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"

namespace crosapi {

namespace {

std::string GetLacrosComponentName() {
  return browser_util::GetLacrosComponentInfo().name;
}

// Returns whether lacros-chrome component is registered.
bool CheckRegisteredMayBlock(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  return manager->IsRegisteredMayBlock(GetLacrosComponentName());
}

// Checks the local disk structure to confirm whether a component is installed.
// We intentionally avoid going through CrOSComponentManager since the latter
// functions around the idea of "registration" -- but the timing of this method
// is prelogin, so the component might exist but not yet be registered.
bool IsInstalledMayBlock(const std::string& name) {
  base::FilePath root;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER, &root)) {
    return false;
  }

  base::FilePath component_root =
      root.Append(component_updater::kComponentsRootPath).Append(name);
  if (!base::PathExists(component_root)) {
    return false;
  }

  // Check for any subdirectory
  base::FileEnumerator enumerator(component_root, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath path = enumerator.Next();
  return !path.empty();
}

// Called after preloading is finished.
void DonePreloading(component_updater::CrOSComponentManager::Error error,
                    const base::FilePath& path) {
  LOG(WARNING) << "Done preloading stateful Lacros. " << static_cast<int>(error)
               << " " << path;
}

// Preloads the given component, or does nothing if |component| is empty.
// Must be called on main thread.
void PreloadComponent(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    std::string component) {
  if (!component.empty()) {
    LOG(WARNING) << "Preloading stateful lacros. " << component;
    manager->Load(
        component, component_updater::CrOSComponentManager::MountPolicy::kMount,
        component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
        base::BindOnce(&DonePreloading));
  }
}

// This method is dispatched pre-login. At this time, we don't know whether
// Lacros is enabled. This method checks to see if the Lacros stateful component
// matching the ash channel is installed -- if it is then Lacros is enabled. At
// which point this method will begin loading stateful lacros.
// Returns the name of the component on success, empty string on failure.
std::string CheckForComponentToPreloadMayBlock() {
  browser_util::ComponentInfo info =
      browser_util::GetLacrosComponentInfoForChannel(chrome::GetChannel());
  bool registered = IsInstalledMayBlock(info.name);
  if (registered) {
    return info.name;
  }
  return "";
}

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!CheckRegisteredMayBlock(manager)) {
    return false;
  }

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros. Skip if Chrome is in safe mode to avoid deleting of
  // user data when Lacros is disabled only temporarily.
  // TODO(hidehiko): This approach has timing issue. Specifically, if Chrome
  // shuts down during the directory remove, some partially-removed directory
  // may be kept, and if the user flips the flag in the next time, that
  // partially-removed directory could be used. Fix this.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode)) {
    // If backward migration is enabled, don't remove the lacros folder as it
    // will used by the migration and will be removed after it completes.
    if (!ash::BrowserDataBackMigrator::IsBackMigrationEnabled(
            crosapi::browser_util::PolicyInitState::kBeforeInit)) {
      base::DeletePathRecursively(browser_util::GetUserDataDir());
    }
  }
  return true;
}

}  // namespace

StatefulLacrosLoader::StatefulLacrosLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : StatefulLacrosLoader(manager, g_browser_process->component_updater()) {}

StatefulLacrosLoader::StatefulLacrosLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    component_updater::ComponentUpdateService* updater)
    : component_manager_(manager), component_update_service_(updater) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckForComponentToPreloadMayBlock),
      base::BindOnce(&PreloadComponent, component_manager_));
  DCHECK(component_manager_);
}

StatefulLacrosLoader::~StatefulLacrosLoader() = default;

void StatefulLacrosLoader::Load(LoadCompletionCallback callback) {
  LOG(WARNING) << "Loading stateful lacros.";
  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      // If a compatible installation exists, use that and download any updates
      // in the background.
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      // If `callback` is null, means stateful lacros-chrome should be
      // installed/updated but rootfs lacros-chrome will be used.
      base::BindOnce(&StatefulLacrosLoader::OnLoad, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void StatefulLacrosLoader::Unload() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_),
      base::BindOnce(&StatefulLacrosLoader::OnCheckInstalledToUnload,
                     weak_factory_.GetWeakPtr()));
}

void StatefulLacrosLoader::Reset() {
  // TODO(crbug.com/1432069): Reset call while loading breaks the behavior. Need
  // to handle such edge cases.
  version_ = absl::nullopt;
}

void StatefulLacrosLoader::GetVersion(
    base::OnceCallback<void(base::Version)> callback) {
  // If version is already calculated, immediately return the cached value.
  // Calculate if not.
  // Note that version value is reset on reloading.
  if (version_.has_value()) {
    std::move(callback).Run(version_.value());
    return;
  }

  // If there currently isn't a stateful lacros-chrome binary, set `verison_`
  // null to proceed to use the rootfs lacros-chrome binary and start the
  // installation of the stateful lacros-chrome binary in the background.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&IsInstalledMayBlock, GetLacrosComponentName()),
      base::BindOnce(&StatefulLacrosLoader::OnCheckInstalledToGetVersion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void StatefulLacrosLoader::OnCheckInstalledToGetVersion(
    base::OnceCallback<void(base::Version)> callback,
    bool is_installed) {
  if (!is_installed) {
    version_ = base::Version();
    std::move(callback).Run(version_.value());
    return;
  }

  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&StatefulLacrosLoader::OnLoad, weak_factory_.GetWeakPtr(),
                     base::BindOnce(
                         [](base::OnceCallback<void(base::Version)> callback,
                            base::Version version, const base::FilePath&) {
                           std::move(callback).Run(version);
                         },
                         std::move(callback))));
}

void StatefulLacrosLoader::OnLoad(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  bool is_stateful_lacros_available =
      error == component_updater::CrOSComponentManager::Error::NONE &&
      !path.empty();
  LOG_IF(WARNING, !is_stateful_lacros_available)
      << "Error loading lacros component image: " << static_cast<int>(error)
      << ", " << path;

  // Update `version_` only when it is not yet calculated.
  // To update `version_` you need to explicitly call `CalculateVersion()`
  // instead of just calling `Load` tlo avoid unnecessary recalculation.
  if (!version_.has_value()) {
    if (is_stateful_lacros_available) {
      version_ = browser_util::GetInstalledLacrosComponentVersion(
          component_update_service_);
    } else {
      version_ = base::Version();
    }
  }

  if (callback) {
    std::move(callback).Run(version_.value(), path);
  }
}

void StatefulLacrosLoader::OnCheckInstalledToUnload(bool was_installed) {
  if (!was_installed) {
    return;
  }

  // Workaround for login crash when the user un-sets the LacrosSupport flag.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  ash::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &StatefulLacrosLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void StatefulLacrosLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  CHECK(ash::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(GetLacrosComponentName());
}

}  // namespace crosapi
