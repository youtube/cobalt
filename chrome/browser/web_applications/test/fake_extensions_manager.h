// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_

#include <unordered_map>
#include <unordered_set>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-forward.h"

namespace web_app {

struct ShortcutInfo;

// This class can be used to 'fake' the ExtensionsManager in tests, which
// attempts to wrap the extensions dependency functionality used by the
// WebAppProvider system.
// TODO(http://crbug.com/454081171): Move tests to use this fake, and implement
// more of the functionality in the fake.
class FakeExtensionsManager : public ExtensionsManager {
 public:
  FakeExtensionsManager();
  ~FakeExtensionsManager() override;

  void SetExtensionsSystemReady(bool ready);
  void SetIsolatedStoragePaths(std::unordered_set<base::FilePath> paths);

  void SetExtensionBlockedByPolicy(const std::string& extension_id,
                                   bool blocked);
  void SetExtensionInstalled(const std::string& extension_id, bool installed);
  void SetExtensionForceInstalled(
      const std::string& extension_id,
      bool force_installed,
      const std::u16string& reason = std::u16string());
  void SetExtensionDefaultInstalled(const std::string& extension_id,
                                    bool default_installed);
  void SetExternalExtensionUninstalled(const std::string& extension_id,
                                       bool uninstalled);
  void SetDidPreinstalledAppsPerformNewInstallation(bool perform);
  void SetPreinstalledExtensionAppId(const std::string& app_id,
                                     bool is_preinstalled);
  void SetUserDisplayMode(const std::string& extension_id,
                          mojom::UserDisplayMode user_display_mode);

  // ExtensionsManager:
  void OnExtensionSystemReady(base::OnceClosure) override;
  std::unordered_set<base::FilePath> GetIsolatedStoragePaths() override;
  std::unique_ptr<ExtensionInstallGate> RegisterGarbageCollectionInstallGate()
      override;

  bool IsExtensionBlockedByPolicy(const std::string& extension_id) override;
  bool IsExtensionInstalled(const std::string& extension_id) override;
  bool IsExtensionForceInstalled(const std::string& extension_id,
                                 std::u16string* reason) override;
  bool IsExtensionDefaultInstalled(const std::string& extension_id) override;
  bool IsExternalExtensionUninstalled(const std::string& extension_id) override;
  bool DidPreinstalledAppsPerformNewInstallation() override;
  bool IsPreinstalledExtensionAppId(const std::string& app_id) override;

  void CopyAppSortingLayout(const std::string& from_extension_id,
                            const std::string& to_web_app_id) override;
  mojom::UserDisplayMode GetExtensionUserDisplayMode(
      const std::string& extension_id) override;
  std::unique_ptr<ShortcutInfo> GetExtensionShortcutInfo(
      const std::string& extension_id) override;
  void WaitForExtensionShortcutsDeleted(const std::string& extension_id,
                                        base::OnceClosure callback) override;

 private:
  bool extensions_system_ready_ = true;
  std::vector<base::OnceClosure> ready_waiters_;
  std::unordered_set<base::FilePath> isolated_storage_paths_;

  std::unordered_set<std::string> blocked_by_policy_;
  std::unordered_set<std::string> installed_;
  std::unordered_map<std::string, std::u16string> force_installed_;
  std::unordered_set<std::string> default_installed_;
  std::unordered_set<std::string> external_uninstalled_;
  bool did_perform_new_installation_ = false;
  std::unordered_map<std::string, bool> preinstalled_app_ids_overrides_;
  std::unordered_map<std::string, mojom::UserDisplayMode> user_display_modes_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_
