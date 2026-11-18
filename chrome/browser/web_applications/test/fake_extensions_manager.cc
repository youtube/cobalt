// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_extensions_manager.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

namespace web_app {
namespace {
class FakeExtensionInstallGate : public ExtensionInstallGate {};
}  // namespace

FakeExtensionsManager::FakeExtensionsManager() = default;
FakeExtensionsManager::~FakeExtensionsManager() = default;

void FakeExtensionsManager::SetExtensionsSystemReady(bool ready) {
  extensions_system_ready_ = ready;
  if (extensions_system_ready_) {
    for (base::OnceClosure& closure : ready_waiters_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(closure));
    }
    ready_waiters_.clear();
  }
}
void FakeExtensionsManager::SetIsolatedStoragePaths(
    std::unordered_set<base::FilePath> paths) {
  isolated_storage_paths_ = paths;
}

void FakeExtensionsManager::SetExtensionBlockedByPolicy(
    const std::string& extension_id,
    bool blocked) {
  if (blocked) {
    blocked_by_policy_.insert(extension_id);
  } else {
    blocked_by_policy_.erase(extension_id);
  }
}
void FakeExtensionsManager::SetExtensionInstalled(
    const std::string& extension_id,
    bool installed) {
  if (installed) {
    installed_.insert(extension_id);
  } else {
    installed_.erase(extension_id);
  }
}
void FakeExtensionsManager::SetExtensionForceInstalled(
    const std::string& extension_id,
    bool force_installed,
    const std::u16string& reason) {
  if (force_installed) {
    force_installed_[extension_id] = reason;
  } else {
    force_installed_.erase(extension_id);
  }
}
void FakeExtensionsManager::SetExtensionDefaultInstalled(
    const std::string& extension_id,
    bool default_installed) {
  if (default_installed) {
    default_installed_.insert(extension_id);
  } else {
    default_installed_.erase(extension_id);
  }
}
void FakeExtensionsManager::SetExternalExtensionUninstalled(
    const std::string& extension_id,
    bool uninstalled) {
  if (uninstalled) {
    external_uninstalled_.insert(extension_id);
  } else {
    external_uninstalled_.erase(extension_id);
  }
}
void FakeExtensionsManager::SetDidPreinstalledAppsPerformNewInstallation(
    bool perform) {
  did_perform_new_installation_ = perform;
}
void FakeExtensionsManager::SetPreinstalledExtensionAppId(
    const std::string& app_id,
    bool is_preinstalled) {
  preinstalled_app_ids_overrides_[app_id] = is_preinstalled;
}
void FakeExtensionsManager::SetUserDisplayMode(
    const std::string& extension_id,
    mojom::UserDisplayMode user_display_mode) {
  user_display_modes_[extension_id] = user_display_mode;
}

void FakeExtensionsManager::OnExtensionSystemReady(base::OnceClosure closure) {
  if (extensions_system_ready_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
    return;
  }
  ready_waiters_.push_back(std::move(closure));
}
std::unordered_set<base::FilePath>
FakeExtensionsManager::GetIsolatedStoragePaths() {
  return isolated_storage_paths_;
}
std::unique_ptr<ExtensionInstallGate>
FakeExtensionsManager::RegisterGarbageCollectionInstallGate() {
  return std::make_unique<FakeExtensionInstallGate>();
}

bool FakeExtensionsManager::IsExtensionBlockedByPolicy(
    const std::string& extension_id) {
  return blocked_by_policy_.contains(extension_id);
}
bool FakeExtensionsManager::IsExtensionInstalled(
    const std::string& extension_id) {
  return installed_.contains(extension_id);
}
bool FakeExtensionsManager::IsExtensionForceInstalled(
    const std::string& extension_id,
    std::u16string* reason) {
  auto it = force_installed_.find(extension_id);
  if (it == force_installed_.end()) {
    return false;
  }
  if (reason) {
    *reason = it->second;
  }
  return true;
}
bool FakeExtensionsManager::IsExtensionDefaultInstalled(
    const std::string& extension_id) {
  return default_installed_.contains(extension_id);
}
bool FakeExtensionsManager::IsExternalExtensionUninstalled(
    const std::string& extension_id) {
  return external_uninstalled_.contains(extension_id);
}
bool FakeExtensionsManager::DidPreinstalledAppsPerformNewInstallation() {
  return did_perform_new_installation_;
}
bool FakeExtensionsManager::IsPreinstalledExtensionAppId(
    const std::string& app_id) {
  auto it = preinstalled_app_ids_overrides_.find(app_id);
  if (it != preinstalled_app_ids_overrides_.end()) {
    return it->second;
  }
  return false;
}

void FakeExtensionsManager::CopyAppSortingLayout(
    const std::string& from_extension_id,
    const std::string& to_web_app_id) {
  // No-op.
}

mojom::UserDisplayMode FakeExtensionsManager::GetExtensionUserDisplayMode(
    const std::string& extension_id) {
  auto it = user_display_modes_.find(extension_id);
  if (it != user_display_modes_.end()) {
    return it->second;
  }
  return mojom::UserDisplayMode::kStandalone;
}

std::unique_ptr<ShortcutInfo> FakeExtensionsManager::GetExtensionShortcutInfo(
    const std::string& extension_id) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->app_id = extension_id;
  return shortcut_info;
}

void FakeExtensionsManager::WaitForExtensionShortcutsDeleted(
    const std::string& extension_id,
    base::OnceClosure callback) {
  std::move(callback).Run();
}
}  // namespace web_app
