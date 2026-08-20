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

#pragma once

#include "starboard/configuration.h"

#include <string>

namespace third_party::starboard::rdk::shared::system {

class SystemProperties {
public:
  static void SetSettings(const std::string& json);
  static bool GetSettings(std::string& out_json);
  static bool GetChipset(std::string &out);
  static bool GetFirmwareVersion(std::string &out);
  static bool GetIntegratorName(std::string &out);
  static bool GetBrandName(std::string &out);
  static bool GetModelName(std::string &out);
  static bool GetModelYear(std::string &out);
  static bool GetFriendlyName(std::string &out);
  static bool GetDeviceType(std::string &out);
};

class AdvertisingId {
public:
  static void SetSettings(const std::string& json);
  static bool GetSettings(std::string& out_json);
  static bool GetIfa(std::string &out);
  static bool GetIfaType(std::string &out);
  static bool GetLmtAdTracking(std::string &out);
};

}  // namespace third_party::starboard::rdk::shared::system

namespace starboard {
namespace system {
using ::third_party::starboard::rdk::shared::system::SystemProperties;
using ::third_party::starboard::rdk::shared::system::AdvertisingId;
}  // namespace system
using system::SystemProperties;
using system::AdvertisingId;
}  // namespace starboard
