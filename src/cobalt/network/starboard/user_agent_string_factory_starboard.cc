// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/network/user_agent_string_factory.h"

#include "starboard/log.h"
#include "starboard/string.h"
#include "starboard/system.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace network {

namespace {

#if SB_API_VERSION == SB_EXPERIMENTAL_API_VERSION
const char kStarboardStabilitySuffix[] = "-Experimental";
#elif defined(SB_RELEASE_CANDIDATE_API_VERSION) && \
    SB_API_VERSION >= SB_RELEASE_CANDIDATE_API_VERSION && \
    SB_API_VERSION < SB_EXPERIMENTAL_API_VERSION
const char kStarboardStabilitySuffix[] = "-ReleaseCandidate";
#else
const char kStarboardStabilitySuffix[] = "";
#endif

bool ShouldUsePlatformInfo(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
    case kSbSystemDeviceTypeGameConsole:
    case kSbSystemDeviceTypeOverTheTopBox:
    case kSbSystemDeviceTypeSetTopBox:
    case kSbSystemDeviceTypeTV:
    case kSbSystemDeviceTypeAndroidTV:
      return true;
    case kSbSystemDeviceTypeDesktopPC:
    case kSbSystemDeviceTypeUnknown:
    default:
      return false;
  }
}

class UserAgentStringFactoryStarboard : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryStarboard();
};

UserAgentStringFactoryStarboard::UserAgentStringFactoryStarboard() {
  starboard_version_ = base::StringPrintf("Starboard/%d%s", SB_API_VERSION,
                                          kStarboardStabilitySuffix);

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  os_name_and_version_ = value;

  // Fill platform info if it is a hardware TV device.
  SbSystemDeviceType device_type = SbSystemGetDeviceType();
  if (ShouldUsePlatformInfo(device_type)) {
    platform_info_ = PlatformInfo();

    // Network operator
    result = SbSystemGetProperty(kSbSystemPropertyNetworkOperatorName, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      platform_info_->network_operator = value;
    }

#if SB_API_VERSION >= 5
    result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      aux_field_ = value;
    }
#endif  // SB_API_VERSION >= 5

    // Device Type
    switch (device_type) {
      case kSbSystemDeviceTypeBlueRayDiskPlayer:
        platform_info_->device_type = PlatformInfo::kBlueRayDiskPlayer;
        break;
      case kSbSystemDeviceTypeGameConsole:
        platform_info_->device_type = PlatformInfo::kGameConsole;
        break;
      case kSbSystemDeviceTypeOverTheTopBox:
        platform_info_->device_type = PlatformInfo::kOverTheTopBox;
        break;
      case kSbSystemDeviceTypeSetTopBox:
        platform_info_->device_type = PlatformInfo::kSetTopBox;
        break;
      case kSbSystemDeviceTypeTV:
        platform_info_->device_type = PlatformInfo::kTV;
        break;
      case kSbSystemDeviceTypeAndroidTV:
        platform_info_->device_type = PlatformInfo::kAndroidTV;
        break;
      case kSbSystemDeviceTypeDesktopPC:
      default:
        platform_info_->device_type = PlatformInfo::kInvalidDeviceType;
    }

    // Chipset model number
    result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      platform_info_->chipset_model_number = value;
    }

    // Firmware version
    result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      platform_info_->firmware_version = value;
    }

    // Brand
    result = SbSystemGetProperty(kSbSystemPropertyManufacturerName, value,
                                 kSystemPropertyMaxLength);
    SB_DCHECK(result);
    if (result) {
      platform_info_->brand = value;
    }

    // Model
    result = SbSystemGetProperty(kSbSystemPropertyModelName, value,
                                 kSystemPropertyMaxLength);
    SB_DCHECK(result);
    if (result) {
      platform_info_->model = value;
    }

    // Connection type
    SbSystemConnectionType connection_type = SbSystemGetConnectionType();
    if (connection_type == kSbSystemConnectionTypeWired) {
      platform_info_->connection_type = PlatformInfo::kWiredConnection;
    } else if (connection_type == kSbSystemConnectionTypeWireless) {
      platform_info_->connection_type = PlatformInfo::kWirelessConnection;
    }
  }
}

}  // namespace

scoped_ptr<UserAgentStringFactory>
UserAgentStringFactory::ForCurrentPlatform() {
  return scoped_ptr<UserAgentStringFactory>(
      new UserAgentStringFactoryStarboard());
}

}  // namespace network
}  // namespace cobalt
