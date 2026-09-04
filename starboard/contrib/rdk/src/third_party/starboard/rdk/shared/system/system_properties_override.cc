//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "third_party/starboard/rdk/shared/system/system_properties_override.h"

#include <core/JSON.h>

#include "starboard/common/once.h"
#include <mutex>

#include "third_party/starboard/rdk/shared/log_override.h"

using namespace  WPEFramework;

namespace third_party::starboard::rdk::shared::system {

namespace {

struct SystemPropertiesImpl {
  struct SystemPropertiesData : public Core::JSON::Container {
    SystemPropertiesData()
      : Core::JSON::Container() {
      Add(_T("modelname"), &ModelName);
      Add(_T("brandname"), &BrandName);
      Add(_T("modelyear"), &ModelYear);
      Add(_T("chipsetmodelnumber"), &ChipsetModelNumber);
      Add(_T("firmwareversion"), &FirmwareVersion);
      Add(_T("integratorname"), &IntegratorName);
      Add(_T("friendlyname"), &FriendlyName);
      Add(_T("devicetype"), &DeviceType);
    }
    SystemPropertiesData(const SystemPropertiesData&) = delete;
    SystemPropertiesData& operator=(const SystemPropertiesData&) = delete;

    Core::JSON::String ModelName;
    Core::JSON::String BrandName;
    Core::JSON::String ModelYear;
    Core::JSON::String ChipsetModelNumber;
    Core::JSON::String FirmwareVersion;
    Core::JSON::String IntegratorName;
    Core::JSON::String FriendlyName;
    Core::JSON::String DeviceType;
  };

  void SetSettings(const std::string& json) {
    std::lock_guard lock(mutex_);
    Core::OptionalType<Core::JSON::Error> error;
    if ( !props_.FromString(json, error) ) {
      props_.Clear();
      SB_LOG(ERROR) << "Failed to parse systemproperties settings, error: "
                    << (error.IsSet() ? Core::JSON::ErrorDisplayMessage(error.Value()): "Unknown");
      return;
    }
  }

  bool GetSettings(std::string& out_json) const {
    std::lock_guard lock(mutex_);
    return props_.ToString(out_json);
  }

  bool GetModelName(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.ModelName.IsSet() && !props_.ModelName.Value().empty()) {
      out = props_.ModelName.Value();
      return true;
    }
    return false;
  }

  bool GetBrandName(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.BrandName.IsSet() && !props_.BrandName.Value().empty()) {
      out = props_.BrandName.Value();
      return true;
    }
    return false;
  }

  bool GetModelYear(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.ModelYear.IsSet() && !props_.ModelYear.Value().empty()) {
      out = props_.ModelYear.Value();
      return true;
    }
    return false;
  }

  bool GetChipset(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.ChipsetModelNumber.IsSet() && !props_.ChipsetModelNumber.Value().empty()) {
      out = props_.ChipsetModelNumber.Value();
      return true;
    }
    return false;
  }

  bool GetFirmwareVersion(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.FirmwareVersion.IsSet() && !props_.FirmwareVersion.Value().empty()) {
      out = props_.FirmwareVersion.Value();
      return true;
    }
    return false;
  }

  bool GetIntegratorName(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.IntegratorName.IsSet() && !props_.IntegratorName.Value().empty()) {
      out = props_.IntegratorName.Value();
      return true;
    }
    return false;
  }

  bool GetFriendlyName(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.FriendlyName.IsSet() && !props_.FriendlyName.Value().empty()) {
      out = props_.FriendlyName.Value();
      return true;
    }
    return false;
  }

  bool GetDeviceType(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.DeviceType.IsSet() && !props_.DeviceType.Value().empty()) {
      out = props_.DeviceType.Value();
      return true;
    }
    return false;
  }

private:
  mutable std::mutex mutex_;
  SystemPropertiesData props_;
};

SB_ONCE_INITIALIZE_FUNCTION(SystemPropertiesImpl, GetSystemProperties);

struct AdvertisingIdImpl {
  struct AdvertisingData : public Core::JSON::Container {
    AdvertisingData()
      : Core::JSON::Container() {
      Add(_T("ifa"), &Ifa);
      Add(_T("ifa_type"), &IfaType);
      Add(_T("lmt"), &Lmt);
    }
    AdvertisingData(const AdvertisingData&) = delete;
    AdvertisingData& operator=(const AdvertisingData&) = delete;

    Core::JSON::String Ifa;
    Core::JSON::String IfaType;
    Core::JSON::String Lmt;
  };

  void SetSettings(const std::string& json) {
    std::lock_guard lock(mutex_);
    Core::OptionalType<Core::JSON::Error> error;
    if ( !props_.FromString(json, error) ) {
      props_.Clear();
      SB_LOG(ERROR) << "Failed to parse advertisingid settings, error: "
                    << (error.IsSet() ? Core::JSON::ErrorDisplayMessage(error.Value()): "Unknown");
      return;
    }
  }

  bool GetSettings(std::string& out_json) const {
    std::lock_guard lock(mutex_);
    return props_.ToString(out_json);
  }

  bool GetIfa(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.Ifa.IsSet() && !props_.Ifa.Value().empty()) {
      out = props_.Ifa.Value();
      return true;
    }
    return false;
  }

  bool GetIfaType(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.IfaType.IsSet() && !props_.IfaType.Value().empty()) {
      out = props_.IfaType.Value();
      return true;
    }
    return false;
  }

  bool GetLmtAdTracking(std::string &out) const {
    std::lock_guard lock(mutex_);
    if (props_.Lmt.IsSet() && !props_.Lmt.Value().empty()) {
      out = props_.Lmt.Value();
      return true;
    }
    return false;
  }

private:
  mutable std::mutex mutex_;
  AdvertisingData props_;
};

SB_ONCE_INITIALIZE_FUNCTION(AdvertisingIdImpl, GetAdvertisingProperties);

}

void SystemProperties::SetSettings(const std::string& json) {
  GetSystemProperties()->SetSettings(json);
}

bool SystemProperties::GetSettings(std::string& out_json) {
  return GetSystemProperties()->GetSettings(out_json);
}

bool SystemProperties::GetChipset(std::string &out) {
  return GetSystemProperties()->GetChipset(out);
}

bool SystemProperties::GetFirmwareVersion(std::string &out) {
  return GetSystemProperties()->GetFirmwareVersion(out);
}

bool SystemProperties::GetIntegratorName(std::string &out) {
  return GetSystemProperties()->GetIntegratorName(out);
}

bool SystemProperties::GetBrandName(std::string &out) {
  return GetSystemProperties()->GetBrandName(out);
}

bool SystemProperties::GetModelName(std::string &out) {
  return GetSystemProperties()->GetModelName(out);
}

bool SystemProperties::GetModelYear(std::string &out) {
  return GetSystemProperties()->GetModelYear(out);
}

bool SystemProperties::GetFriendlyName(std::string &out) {
  return GetSystemProperties()->GetFriendlyName(out);
}

bool SystemProperties::GetDeviceType(std::string &out) {
  return GetSystemProperties()->GetDeviceType(out);
}

void AdvertisingId::SetSettings(const std::string& json) {
  GetAdvertisingProperties()->SetSettings(json);
}

bool AdvertisingId::GetSettings(std::string& out_json) {
  return GetAdvertisingProperties()->GetSettings(out_json);
}

bool AdvertisingId::GetIfa(std::string& out_json) {
  return GetAdvertisingProperties()->GetIfa(out_json);
}

bool AdvertisingId::GetIfaType(std::string& out_json) {
  return GetAdvertisingProperties()->GetIfaType(out_json);
}

bool AdvertisingId::GetLmtAdTracking(std::string& out_json) {
  return GetAdvertisingProperties()->GetLmtAdTracking(out_json);
}

}
