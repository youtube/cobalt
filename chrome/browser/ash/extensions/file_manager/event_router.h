// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/device_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_watcher.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefChangeRegistrar;
class Profile;

using OutputsType =
    extensions::api::file_manager_private::ProgressStatus::OutputsType;
using file_manager::util::EntryDefinition;

namespace file_manager {

namespace {
class RecalculateTasksObserver;
}  // namespace

// Monitors changes in disk mounts, network connection state and preferences
// affecting File Manager. Dispatches appropriate File Browser events.
class EventRouter
    : public KeyedService,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public extensions::ExtensionRegistryObserver,
      public ash::system::TimezoneSettings::Observer,
      public VolumeManagerObserver,
      public arc::ArcIntentHelperObserver,
      public drive::DriveIntegrationServiceObserver,
      public guest_os::GuestOsSharePath::Observer,
      public ash::TabletModeObserver,
      public file_manager::io_task::IOTaskController::Observer,
      public guest_os::GuestOsMountProviderRegistry::Observer {
 public:
  using DispatchDirectoryChangeEventImplCallback =
      base::RepeatingCallback<void(const base::FilePath& virtual_path,
                                   bool got_error,
                                   const std::vector<url::Origin>& listeners)>;

  explicit EventRouter(Profile* profile);

  EventRouter(const EventRouter&) = delete;
  EventRouter& operator=(const EventRouter&) = delete;

  ~EventRouter() override;

  // arc::ArcIntentHelperObserver overrides.
  void OnIntentFiltersUpdated(
      const absl::optional<std::string>& package_name) override;

  // KeyedService overrides.
  void Shutdown() override;

  using BoolCallback = base::OnceCallback<void(bool success)>;

  // Adds a file watch at |local_path|, associated with |virtual_path|, for
  // an listener with |listener_origin|.
  //
  // |callback| will be called with true on success, or false on failure.
  // |callback| must not be null.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void AddFileWatch(const base::FilePath& local_path,
                    const base::FilePath& virtual_path,
                    const url::Origin& listener_origin,
                    BoolCallback callback);

  // Removes a file watch at |local_path| for listener with |listener_origin|.
  //
  // Obsolete. Used as fallback for files which backends do not implement the
  // storage::WatcherManager interface.
  void RemoveFileWatch(const base::FilePath& local_path,
                       const url::Origin& listener_origin);

  // Called when a notification from a watcher manager arrives.
  void OnWatcherManagerNotification(
      const storage::FileSystemURL& file_system_url,
      const url::Origin& listener_origin,
      storage::WatcherManager::ChangeType change_type);

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // extensions::ExtensionRegistryObserver overrides
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // ash::system::TimezoneSettings::Observer overrides.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // VolumeManagerObserver overrides.
  void OnDiskAdded(const ash::disks::Disk& disk, bool mounting) override;
  void OnDiskRemoved(const ash::disks::Disk& disk) override;
  void OnDeviceAdded(const std::string& device_path) override;
  void OnDeviceRemoved(const std::string& device_path) override;
  void OnVolumeMounted(ash::MountError error_code,
                       const Volume& volume) override;
  void OnVolumeUnmounted(ash::MountError error_code,
                         const Volume& volume) override;
  void OnFormatStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnFormatCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;
  void OnPartitionStarted(const std::string& device_path,
                          const std::string& device_label,
                          bool success) override;
  void OnPartitionCompleted(const std::string& device_path,
                            const std::string& device_label,
                            bool success) override;
  void OnRenameStarted(const std::string& device_path,
                       const std::string& device_label,
                       bool success) override;
  void OnRenameCompleted(const std::string& device_path,
                         const std::string& device_label,
                         bool success) override;
  // Set custom dispatch directory change event implementation for testing.
  void SetDispatchDirectoryChangeEventImplForTesting(
      const DispatchDirectoryChangeEventImplCallback& callback);

  // DriveIntegrationServiceObserver override.
  void OnFileSystemMountFailed() override;

  // guest_os::GuestOsSharePath::Observer overrides.
  void OnPersistedPathRegistered(const std::string& vm_name,
                                 const base::FilePath& path) override;
  void OnUnshare(const std::string& vm_name,
                 const base::FilePath& path) override;
  void OnGuestRegistered(const guest_os::GuestId& guest) override;
  void OnGuestUnregistered(const guest_os::GuestId& guest) override;

  // ash:TabletModeObserver overrides.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Notifies FilesApp that file drop to Plugin VM was not in a shared directory
  // and failed FilesApp will show the "Move to Windows files" dialog.
  void DropFailedPluginVmDirectoryNotShared();

  // Called by the UI to notify the result of a displayed dialog.
  void OnDriveDialogResult(drivefs::mojom::DialogResult result);

  // Returns a weak pointer for the event router.
  base::WeakPtr<EventRouter> GetWeakPtr();

  // IOTaskController::Observer:
  void OnIOTaskStatus(const io_task::ProgressStatus& status) override;

  // guest_os::GuestOsMountProviderRegistry::Observer overrides.
  void OnRegistered(guest_os::GuestOsMountProviderRegistry::Id id,
                    guest_os::GuestOsMountProvider* provider) override;
  void OnUnregistered(guest_os::GuestOsMountProviderRegistry::Id id) override;

  // Broadcast to Files app frontend that file tasks might have changed.
  void BroadcastOnAppsUpdatedEvent();

  // Use this method for unit tests to bypass checking if there are any SWA
  // windows.
  void ForceBroadcastingForTesting(bool enabled) {
    force_broadcasting_for_testing_ = enabled;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EventRouterTest, PopulateCrostiniEvent);
  friend class ScopedSuppressDriveNotificationsForPath;

  // Starts observing file system change events.
  void ObserveEvents();

  // Called when prefs related to file manager change.
  void OnFileManagerPrefsChanged();

  // Process file watch notifications.
  void HandleFileWatchNotification(const base::FilePath& path, bool got_error);

  // Sends directory change event.
  void DispatchDirectoryChangeEvent(const base::FilePath& path,
                                    bool got_error,
                                    const std::vector<url::Origin>& listeners);

  // Default implementation of DispatchDirectoryChangeEvent.
  void DispatchDirectoryChangeEventImpl(
      const base::FilePath& path,
      bool got_error,
      const std::vector<url::Origin>& listeners);

  // Sends directory change event, after converting the file definition to entry
  // definition.
  void DispatchDirectoryChangeEventWithEntryDefinition(
      bool watcher_error,
      const EntryDefinition& entry_definition);

  // Dispatches the mount completed event.
  void DispatchMountCompletedEvent(
      extensions::api::file_manager_private::MountCompletedEventType event_type,
      ash::MountError error,
      const Volume& volume);

  // Send crostini path shared or unshared event.
  void SendCrostiniEvent(
      extensions::api::file_manager_private::CrostiniEventType event_type,
      const std::string& vm_name,
      const base::FilePath& path);

  // Populate the crostini path shared or unshared event.
  static void PopulateCrostiniEvent(
      extensions::api::file_manager_private::CrostiniEvent& event,
      extensions::api::file_manager_private::CrostiniEventType event_type,
      const std::string& vm_name,
      const url::Origin& origin,
      const std::string& mount_name,
      const std::string& file_system_name,
      const std::string& full_path);

  void NotifyDriveConnectionStatusChanged();

  void DisplayDriveConfirmDialog(
      const drivefs::mojom::DialogReason& reason,
      base::OnceCallback<void(drivefs::mojom::DialogResult)> callback);

  // Used by `file_manager::ScopedSuppressDriveNotificationsForPath` to prevent
  // Drive notifications for a given file identified by its relative Drive path.
  void SuppressDriveNotificationsForFilePath(
      const base::FilePath& relative_drive_path);
  void RestoreDriveNotificationsForFilePath(
      const base::FilePath& relative_drive_path);

  // Called to refresh the list of guests and broadcast it.
  void OnMountableGuestsChanged();

  // After resolving all file definitions, ensure they are available on the
  // `event_status`.
  void OnConvertFileDefinitionListToEntryDefinitionList(
      file_manager_private::ProgressStatus event_status,
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);

  // Broadcast the `event_status` to all open SWA windows.
  void BroadcastIOTask(
      const file_manager_private::ProgressStatus& event_status);

  std::map<base::FilePath, std::unique_ptr<FileWatcher>> file_watchers_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<Profile, ExperimentalAsh> profile_;

  std::unique_ptr<SystemNotificationManager> notification_manager_;
  std::unique_ptr<DeviceEventRouter> device_event_router_;
  std::unique_ptr<DriveFsEventRouter> drivefs_event_router_;
  std::unique_ptr<RecalculateTasksObserver> recalculate_tasks_observer_;

  DispatchDirectoryChangeEventImplCallback
      dispatch_directory_change_event_impl_;

  // Set this to true to ignore the DoFilesSwaWindowsExist check for testing.
  bool force_broadcasting_for_testing_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EventRouter> weak_factory_{this};
};

file_manager_private::MountError MountErrorToMountCompletedStatus(
    ash::MountError error);

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_H_
