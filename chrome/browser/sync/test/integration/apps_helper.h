// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/sync/model/string_ordinal.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace apps_helper {

// Returns true iff |profile1| has same apps (hosted, legacy packaged and
// platform) as |profile2|.
bool HasSameApps(Profile* profile1, Profile* profile2);

// Returns true iff all existing profiles have the same apps (hosted,
// legacy packaged and platform).
[[nodiscard]] bool AllProfilesHaveSameApps();

// Installs the hosted app for the given index to |profile|, and returns the
// extension ID of the new app.
std::string InstallHostedApp(Profile* profile, int index);

// Installs the platform app for the given index to |profile|, and returns the
// extension ID of the new app. Indices passed to this method should be distinct
// from indices passed to InstallApp.
std::string InstallPlatformApp(Profile* profile, int index);

// Installs the hosted app for the given index to all profiles (including the
// verifier), and returns the extension ID of the new app.
std::string InstallHostedAppForAllProfiles(int index);

// Installs the web app for the given WebAppInstallInfo and profile. This does
// not download icons or run OS integration installs.
webapps::AppId InstallWebApp(Profile* profile,
                             const web_app::WebAppInstallInfo& info);

// Uninstalls the app for the given index from |profile|. Assumes that it was
// previously installed.
void UninstallApp(Profile* profile, int index);

// Installs all pending synced apps for |profile|, including waiting for the
// App Service to settle.
void InstallAppsPendingForSync(Profile* profile);

// Enables the app for the given index on |profile|.
void EnableApp(Profile* profile, int index);

// Disables the app for the given index on |profile|.
void DisableApp(Profile* profile, int index);

// Returns true if the app with index |index| is enabled on |profile|.
bool IsAppEnabled(Profile* profile, int index);

// Enables the app for the given index in incognito mode on |profile|.
void IncognitoEnableApp(Profile* profile, int index);

// Disables the app for the given index in incognito mode on |profile|.
void IncognitoDisableApp(Profile* profile, int index);

// Returns true if the app with index |index| is enabled in incognito mode on
// |profile|.
bool IsIncognitoEnabled(Profile* profile, int index);

// Gets the page ordinal value for the application at the given index on
// |profile|.
syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile, int app_index);

// Sets a new |page_ordinal| value for the application at the given index
// on |profile|.
void SetPageOrdinalForApp(Profile* profile,
                          int app_index,
                          const syncer::StringOrdinal& page_ordinal);

// Gets the app launch ordinal value for the application at the given index on
// |profile|.
syncer::StringOrdinal GetAppLaunchOrdinalForApp(Profile* profile,
                                                int app_index);

// Sets a new |page_ordinal| value for the application at the given index
// on |profile|.
void SetAppLaunchOrdinalForApp(Profile* profile,
                               int app_index,
                               const syncer::StringOrdinal& app_launch_ordinal);

// Copy the page and app launch ordinal value for the application at the given
// index on |profile_source| to |profile_destination|.
// The main intention of this is to properly setup the values on the verifier
// profile in situations where the other profiles have conflicting values.
void CopyNTPOrdinals(Profile* source, Profile* destination, int index);

// Fix any NTP icon collisions that are currently in |profile|.
void FixNTPOrdinalCollisions(Profile* profile);

// Flushes pending changes and verifies that the profiles have no pending
// installs or uninstalls afterwards.
bool AwaitWebAppQuiescence(std::vector<Profile*> profiles);
}  // namespace apps_helper

// An app specific version of StatusChangeChecker which checks the exit
// condition on all extension app events. Subclasses must override
// IsExitConditionSatisfied() with their desired check.
class AppsStatusChangeChecker : public StatusChangeChecker,
                                public extensions::ExtensionRegistryObserver,
                                public extensions::ExtensionPrefsObserver,
                                public extensions::InstallObserver {
 public:
  AppsStatusChangeChecker();

  AppsStatusChangeChecker(const AppsStatusChangeChecker&) = delete;
  AppsStatusChangeChecker& operator=(const AppsStatusChangeChecker&) = delete;

  ~AppsStatusChangeChecker() override;

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // extensions::ExtensionPrefsObserver implementation.
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disabled_reasons) override;
  void OnExtensionRegistered(const std::string& extension_id,
                             const base::Time& install_time,
                             bool is_enabled) override;
  void OnExtensionPrefsLoaded(const std::string& extension_id,
                              const extensions::ExtensionPrefs* prefs) override;
  void OnExtensionPrefsDeleted(const std::string& extension_id) override;
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool state) override;

  // Implementation of extensions::InstallObserver.
  void OnAppsReordered(
      content::BrowserContext* context,
      const absl::optional<std::string>& extension_id) override;

 protected:
  std::vector<Profile*> profiles_;

 private:
  void InstallSyncedApps(Profile* profile);

  content::NotificationRegistrar registrar_;

  base::ScopedMultiSourceObservation<extensions::InstallTracker,
                                     extensions::InstallObserver>
      install_tracker_observation_{this};

  base::WeakPtrFactory<AppsStatusChangeChecker> weak_ptr_factory_{this};
};

// Checker to block for a set of profiles to have matching extensions lists. If
// the verifier profile is enabled, it will be included in the set of profiles
// to check against.
class AppsMatchChecker : public AppsStatusChangeChecker {
 public:
  AppsMatchChecker();

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_
