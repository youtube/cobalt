// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/check.h"
#include "base/observer_list.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/themes/cross_device/theme_comparer.h"

namespace themes {

syncer::DataType OsTypeToDataType(syncer::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case syncer::DeviceInfo::OsType::kAndroid:
      return syncer::THEMES_ANDROID;
    case syncer::DeviceInfo::OsType::kIOS:
      return syncer::THEMES_IOS;
    default:
      return syncer::UNSPECIFIED;
  }
}

// DeviceThemeInfo implementation:

template <typename LocalSpecifics>
DeviceThemeInfo<LocalSpecifics>::DeviceThemeInfo() = default;

template <typename LocalSpecifics>
DeviceThemeInfo<LocalSpecifics>::DeviceThemeInfo(const DeviceThemeInfo&) =
    default;

template <typename LocalSpecifics>
DeviceThemeInfo<LocalSpecifics>& DeviceThemeInfo<LocalSpecifics>::operator=(
    const DeviceThemeInfo&) = default;

template <typename LocalSpecifics>
DeviceThemeInfo<LocalSpecifics>::~DeviceThemeInfo() = default;

template <typename LocalSpecifics>
bool DeviceThemeInfo<LocalSpecifics>::operator==(
    const DeviceThemeInfo& other) const {
  return device_name == other.device_name && os_type == other.os_type &&
         form_factor == other.form_factor &&
         ThemeComparer<LocalSpecifics>::Equals(theme, other.theme);
}

// CrossDeviceThemeTracker implementation:

template <typename LocalSpecifics>
CrossDeviceThemeTracker<LocalSpecifics>::CrossDeviceThemeTracker(
    syncer::DeviceInfoTracker* device_info_tracker)
    : device_info_tracker_(device_info_tracker) {
  if (device_info_tracker_) {
    device_info_tracker_->AddObserver(this);
  }
}

template <typename LocalSpecifics>
CrossDeviceThemeTracker<LocalSpecifics>::~CrossDeviceThemeTracker() {
  if (device_info_tracker_) {
    device_info_tracker_->RemoveObserver(this);
  }
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

template <typename LocalSpecifics>
std::vector<DeviceThemeInfo<LocalSpecifics>>
CrossDeviceThemeTracker<LocalSpecifics>::GetOtherDevicesThemes() const {
  std::vector<DeviceThemeInfo<LocalSpecifics>> themes;
  for (const auto& [_, theme_info] : other_themes_) {
    themes.push_back(theme_info);
  }
  return themes;
}

template <typename LocalSpecifics>
ServiceStatus CrossDeviceThemeTracker<LocalSpecifics>::GetServiceStatus()
    const {
  return status_;
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::RegisterBridge(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeSyncBridge> bridge) {
  DCHECK(bridge);
  DCHECK(bridges_.find(type) == bridges_.end());
  bridges_[type] = std::move(bridge);
}

template <typename LocalSpecifics>
base::WeakPtr<syncer::DataTypeControllerDelegate>
CrossDeviceThemeTracker<LocalSpecifics>::GetSyncDelegateForType(
    syncer::DataType type) {
  auto it = bridges_.find(type);
  if (it == bridges_.end()) {
    return nullptr;
  }
  return it->second->change_processor()->GetControllerDelegate();
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::UpdateThemeInfo(
    const std::string& cache_guid,
    DeviceThemeInfo<LocalSpecifics> theme_info) {
  ResolveDeviceInfo(cache_guid, theme_info);
  auto it = other_themes_.find(cache_guid);
  if (it != other_themes_.end() && it->second == theme_info) {
    return;
  }
  other_themes_[cache_guid] = std::move(theme_info);
  NotifyObservers();
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::RemoveThemeInfo(
    const std::string& cache_guid) {
  if (other_themes_.erase(cache_guid) > 0) {
    NotifyObservers();
  }
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeStatusChanged(
    syncer::DataType type,
    ServiceStatus status) {
  bridge_statuses_[type] = status;
  UpdateGlobalStatus();
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::ClearAllThemeInfo() {
  other_themes_.clear();
  NotifyObservers();
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::Shutdown() {
  if (device_info_tracker_) {
    device_info_tracker_->RemoveObserver(this);
    device_info_tracker_ = nullptr;
  }
  bridges_.clear();
  observers_.Clear();
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::OnDeviceInfoChange() {
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

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::SetStatus(ServiceStatus status) {
  if (status_ == status) {
    return;
  }
  status_ = status;
  for (auto& observer : observers_) {
    observer.OnServiceStatusChanged(status_);
  }
}

template <typename LocalSpecifics>
syncer::DeviceInfoTracker*
CrossDeviceThemeTracker<LocalSpecifics>::device_info_tracker() {
  return device_info_tracker_;
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnCrossDeviceThemeChanged();
  }
}

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::ResolveDeviceInfo(
    const std::string& client_tag_hash_value,
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

template <typename LocalSpecifics>
void CrossDeviceThemeTracker<LocalSpecifics>::UpdateGlobalStatus() {
  // Wait for all registered bridges to report their status.
  if (bridge_statuses_.size() < bridges_.size()) {
    SetStatus(ServiceStatus::kInitializing);
    return;
  }

  if (bridge_statuses_.empty()) {
    SetStatus(ServiceStatus::kInitializing);
    return;
  }

  bool any_active = false;
  bool all_disabled = true;

  for (const auto& [_, status] : bridge_statuses_) {
    if (status == ServiceStatus::kActive) {
      any_active = true;
    }
    if (status != ServiceStatus::kSyncDisabled) {
      all_disabled = false;
    }
  }

  if (any_active) {
    SetStatus(ServiceStatus::kActive);
  } else if (all_disabled) {
    SetStatus(ServiceStatus::kSyncDisabled);
  } else {
    SetStatus(ServiceStatus::kInitializing);
  }
}

// Explicit instantiations.
template struct DeviceThemeInfo<sync_pb::ThemeSpecifics>;
template class CrossDeviceThemeTracker<sync_pb::ThemeSpecifics>;

}  // namespace themes
