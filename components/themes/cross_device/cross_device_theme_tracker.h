// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace themes {

enum class ServiceStatus {
  kInitializing,
  kActive,
  kSyncDisabled,
};

// Helper to compare theme specifics.
// Each platform must specialize this for its own specifics type to avoid
// using SerializeAsString() which is unstable.
template <typename T>
struct ThemeComparer;

// Holds theme information from a specific platform and device.
template <typename LocalSpecifics>
struct DeviceThemeInfo {
  DeviceThemeInfo() = default;
  DeviceThemeInfo(const DeviceThemeInfo&) = default;
  DeviceThemeInfo& operator=(const DeviceThemeInfo&) = default;
  ~DeviceThemeInfo() = default;

  bool operator==(const DeviceThemeInfo& other) const {
    return device_name == other.device_name && os_type == other.os_type &&
           form_factor == other.form_factor &&
           ThemeComparer<LocalSpecifics>::Equals(theme, other.theme);
  }

  std::string device_name;
  syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kUnknown;
  syncer::DeviceInfo::FormFactor form_factor =
      syncer::DeviceInfo::FormFactor::kUnknown;
  LocalSpecifics theme;
};

// Helper to map DeviceInfo OS type to Sync DataType.
inline syncer::DataType OsTypeToDataType(syncer::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case syncer::DeviceInfo::OsType::kAndroid:
      return syncer::THEMES_ANDROID;
    case syncer::DeviceInfo::OsType::kIOS:
      return syncer::THEMES_IOS;
    case syncer::DeviceInfo::OsType::kWindows:
    case syncer::DeviceInfo::OsType::kMac:
    case syncer::DeviceInfo::OsType::kLinux:
    case syncer::DeviceInfo::OsType::kChromeOsAsh:
    case syncer::DeviceInfo::OsType::kChromeOsLacros:
      return syncer::THEMES;
    default:
      return syncer::UNSPECIFIED;
  }
}

// Base class for tracking theme configurations across devices.
// It maintains a cache of themes from other devices and notifies observers when
// they change. Derived classes implement platform-specific sync logic.
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
      syncer::DeviceInfoTracker* device_info_tracker)
      : device_info_tracker_(device_info_tracker) {
    if (device_info_tracker_) {
      device_info_tracker_->AddObserver(this);
    }
  }

  CrossDeviceThemeTracker(const CrossDeviceThemeTracker&) = delete;
  CrossDeviceThemeTracker& operator=(const CrossDeviceThemeTracker&) = delete;

  ~CrossDeviceThemeTracker() override {
    if (device_info_tracker_) {
      device_info_tracker_->RemoveObserver(this);
    }
  }

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  std::vector<DeviceThemeInfo<LocalSpecifics>> GetOtherDevicesThemes() const {
    std::vector<DeviceThemeInfo<LocalSpecifics>> themes;
    for (const auto& [_, theme_info] : other_themes_) {
      themes.push_back(theme_info);
    }
    return themes;
  }

  ServiceStatus GetServiceStatus() const { return status_; }

  // KeyedService:
  void Shutdown() override {
    if (device_info_tracker_) {
      device_info_tracker_->RemoveObserver(this);
      device_info_tracker_ = nullptr;
    }
    observers_.Clear();
  }

  // DeviceInfoTracker::Observer:
  void OnDeviceInfoChange() override {
    bool changed = false;
    for (auto& [cache_guid, theme_info] : other_themes_) {
      DeviceThemeInfo<LocalSpecifics> updated_info = theme_info;
      ResolveDeviceInfo(cache_guid, updated_info);
      if (theme_info.device_name != updated_info.device_name ||
          theme_info.form_factor != updated_info.form_factor) {
        theme_info.device_name = updated_info.device_name;
        theme_info.form_factor = updated_info.form_factor;
        changed = true;
      }
    }
    if (changed) {
      NotifyObservers();
    }
  }

 protected:
  void UpdateThemeInfo(const std::string& cache_guid,
                       DeviceThemeInfo<LocalSpecifics> theme_info) {
    ResolveDeviceInfo(cache_guid, theme_info);
    auto it = other_themes_.find(cache_guid);
    if (it != other_themes_.end() && it->second == theme_info) {
      return;
    }
    other_themes_[cache_guid] = std::move(theme_info);
    NotifyObservers();
  }

  void RemoveThemeInfo(const std::string& cache_guid) {
    if (other_themes_.erase(cache_guid) > 0) {
      NotifyObservers();
    }
  }

  void SetStatus(ServiceStatus status) {
    if (status_ == status) {
      return;
    }
    status_ = status;
    for (auto& observer : observers_) {
      observer.OnServiceStatusChanged(status_);
    }
  }

  syncer::DeviceInfoTracker* device_info_tracker() {
    return device_info_tracker_;
  }

  void NotifyObservers() {
    for (auto& observer : observers_) {
      observer.OnCrossDeviceThemeChanged();
    }
  }

 private:
  void ResolveDeviceInfo(const std::string& client_tag_hash_value,
                         DeviceThemeInfo<LocalSpecifics>& theme_info) {
    if (!device_info_tracker_) {
      return;
    }
    syncer::DataType type = OsTypeToDataType(theme_info.os_type);
    if (type == syncer::UNSPECIFIED) {
      return;
    }
    for (const auto* device : device_info_tracker_->GetAllDeviceInfo()) {
      auto hash = syncer::ClientTagHash::FromUnhashed(type, device->guid());
      if (hash.value() == client_tag_hash_value) {
        theme_info.device_name = device->client_name();
        theme_info.form_factor = device->form_factor();
        return;
      }
    }
  }

  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  base::ObserverList<Observer> observers_;
  std::map<std::string, DeviceThemeInfo<LocalSpecifics>> other_themes_;
  ServiceStatus status_ = ServiceStatus::kInitializing;
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
