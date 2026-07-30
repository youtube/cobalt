// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace themes {

enum class ServiceStatus {
  kInitializing,
  kActive,
  kSyncDisabled,
};

// Holds theme information from a specific platform and device.
template <typename LocalSpecifics>
struct DeviceThemeInfo {
  DeviceThemeInfo();
  DeviceThemeInfo(const DeviceThemeInfo&);
  DeviceThemeInfo& operator=(const DeviceThemeInfo&);
  ~DeviceThemeInfo();

  bool operator==(const DeviceThemeInfo& other) const;

  std::string device_name;
  syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kUnknown;
  syncer::DeviceInfo::FormFactor form_factor =
      syncer::DeviceInfo::FormFactor::kUnknown;
  LocalSpecifics theme;
};

// Helper to map DeviceInfo OS type to Sync DataType.
syncer::DataType OsTypeToDataType(syncer::DeviceInfo::OsType os_type);

// KeyedService that tracks themes from other devices.
template <typename LocalSpecifics>
class CrossDeviceThemeTracker : public KeyedService,
                                public syncer::DeviceInfoTracker::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCrossDeviceThemeChanged() = 0;
    virtual void OnServiceStatusChanged(ServiceStatus status) = 0;
  };

  explicit CrossDeviceThemeTracker(
      syncer::DeviceInfoTracker* device_info_tracker);

  CrossDeviceThemeTracker(const CrossDeviceThemeTracker&) = delete;
  CrossDeviceThemeTracker& operator=(const CrossDeviceThemeTracker&) = delete;

  ~CrossDeviceThemeTracker() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::vector<DeviceThemeInfo<LocalSpecifics>> GetOtherDevicesThemes() const;
  ServiceStatus GetServiceStatus() const;

  void RegisterBridge(syncer::DataType type,
                      std::unique_ptr<syncer::DataTypeSyncBridge> bridge);

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncDelegateForType(
      syncer::DataType type);

  void UpdateThemeInfo(const std::string& cache_guid,
                       DeviceThemeInfo<LocalSpecifics> theme_info);
  void RemoveThemeInfo(const std::string& cache_guid);
  void OnBridgeStatusChanged(syncer::DataType type, ServiceStatus status);
  void ClearAllThemeInfo();

  // KeyedService:
  void Shutdown() override;

  // DeviceInfoTracker::Observer:
  void OnDeviceInfoChange() override;

 protected:
  void SetStatus(ServiceStatus status);
  syncer::DeviceInfoTracker* device_info_tracker();
  void NotifyObservers();

 private:
  void ResolveDeviceInfo(const std::string& client_tag_hash_value,
                         DeviceThemeInfo<LocalSpecifics>& theme_info);
  void UpdateGlobalStatus();

  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  base::ObserverList<Observer> observers_;
  std::map<std::string, DeviceThemeInfo<LocalSpecifics>> other_themes_;
  std::map<syncer::DataType, ServiceStatus> bridge_statuses_;
  std::map<syncer::DataType, std::unique_ptr<syncer::DataTypeSyncBridge>>
      bridges_;
  ServiceStatus status_ = ServiceStatus::kInitializing;
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
