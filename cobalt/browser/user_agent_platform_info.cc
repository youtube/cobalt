// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/user_agent_platform_info.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#include "cobalt/renderer/get_default_rasterizer_for_platform.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#if SB_IS(EVERGREEN)
#include "cobalt/updater/utils.h"
#endif

namespace cobalt {
namespace browser {

namespace {

std::string CreateDeviceTypeString(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
      return "BDP";
    case kSbSystemDeviceTypeGameConsole:
      return "GAME";
    case kSbSystemDeviceTypeOverTheTopBox:
      return "OTT";
    case kSbSystemDeviceTypeSetTopBox:
      return "STB";
    case kSbSystemDeviceTypeTV:
      return "TV";
    case kSbSystemDeviceTypeAndroidTV:
      return "ATV";
    case kSbSystemDeviceTypeDesktopPC:
      return "DESKTOP";
    case kSbSystemDeviceTypeUnknown:
      return "UNKNOWN";
    default:
      NOTREACHED();
      return "";
  }
}

std::string CreateConnectionTypeString(
    const base::Optional<SbSystemConnectionType>& connection_type) {
  if (connection_type) {
    switch (*connection_type) {
      case kSbSystemConnectionTypeWired:
        return "Wired";
      case kSbSystemConnectionTypeWireless:
        return "Wireless";
      default:
        NOTREACHED();
    }
  }

  return "";
}

}  // namespace

UserAgentPlatformInfo::UserAgentPlatformInfo() { InitializeFields(); }

void UserAgentPlatformInfo::set_device_type(SbSystemDeviceType device_type) {
  device_type_ = device_type;
  device_type_string_ = CreateDeviceTypeString(device_type_);
}

void UserAgentPlatformInfo::set_connection_type(
    base::Optional<SbSystemConnectionType> connection_type) {
  if (connection_type) {
    connection_type_ = connection_type;
    connection_type_string_ = CreateConnectionTypeString(connection_type_);
  } else {
    connection_type_string_ = "";
  }
}

void UserAgentPlatformInfo::InitializeFields() {
  starboard_version_ = base::StringPrintf("Starboard/%d", SB_API_VERSION);

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  os_name_and_version_ = value;

  // Fill platform info if it is a hardware TV device.
  SbSystemDeviceType device_type = SbSystemGetDeviceType();

#if SB_API_VERSION >= 12
  // System Integrator
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName, value,
                               kSystemPropertyMaxLength);
#else
  // Original Design Manufacturer (ODM)
  result = SbSystemGetProperty(kSbSystemPropertyOriginalDesignManufacturerName,
                               value, kSystemPropertyMaxLength);
#endif
  if (result) {
    original_design_manufacturer_ = value;
  }

  javascript_engine_version_ = script::GetJavaScriptEngineNameAndVersion();

  rasterizer_type_ =
      renderer::GetDefaultRasterizerForPlatform().rasterizer_name;

// Evergreen version
#if SB_IS(EVERGREEN)
  evergreen_version_ = updater::GetCurrentEvergreenVersion();
#endif

  cobalt_version_ = COBALT_VERSION;
  cobalt_build_version_number_ = COBALT_BUILD_VERSION_NUMBER;

#if defined(COBALT_BUILD_TYPE_DEBUG)
  build_configuration_ = "debug";
#elif defined(COBALT_BUILD_TYPE_DEVEL)
  build_configuration_ = "devel";
#elif defined(COBALT_BUILD_TYPE_QA)
  build_configuration_ = "qa";
#elif defined(COBALT_BUILD_TYPE_GOLD)
  build_configuration_ = "gold";
#else
#error Unknown build configuration.
#endif

  result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField, value,
                               kSystemPropertyMaxLength);
  if (result) {
    aux_field_ = value;
  }

  // Device Type
  device_type_ = device_type;
  device_type_string_ = CreateDeviceTypeString(device_type_);

  // Chipset model number
  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                               kSystemPropertyMaxLength);
  if (result) {
    chipset_model_number_ = value;
  }

  // Model year
  result = SbSystemGetProperty(kSbSystemPropertyModelYear, value,
                               kSystemPropertyMaxLength);
  if (result) {
    model_year_ = value;
  }

  // Firmware version
  result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                               kSystemPropertyMaxLength);
  if (result) {
    firmware_version_ = value;
  }

  // Brand
  result = SbSystemGetProperty(kSbSystemPropertyBrandName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    brand_ = value;
  }

  // Model name
  result = SbSystemGetProperty(kSbSystemPropertyModelName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    model_ = value;
  }

  // Connection type
  SbSystemConnectionType connection_type = SbSystemGetConnectionType();
  if (connection_type != kSbSystemConnectionTypeUnknown) {
    connection_type_ = connection_type;
  }
  connection_type_string_ = CreateConnectionTypeString(connection_type_);
}

}  // namespace browser
}  // namespace cobalt
