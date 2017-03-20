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

bool SystemDeviceTypeIsTv(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
    case kSbSystemDeviceTypeGameConsole:
    case kSbSystemDeviceTypeOverTheTopBox:
    case kSbSystemDeviceTypeSetTopBox:
    case kSbSystemDeviceTypeTV:
#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
    case kSbSystemDeviceTypeAndroidTV:
#endif  // SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
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
  starboard_version_ = base::StringPrintf("Starboard/%d", SB_API_VERSION);

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  os_name_and_version_ = value;

  // Fill YouTube TV info if it is a YouTube TV device.
  SbSystemDeviceType device_type = SbSystemGetDeviceType();
  if (SystemDeviceTypeIsTv(device_type)) {
    youtube_tv_info_ = YouTubeTVInfo();

    // Network operator
    result = SbSystemGetProperty(kSbSystemPropertyNetworkOperatorName, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      youtube_tv_info_->network_operator = value;
    }

    // Device Type
    switch (device_type) {
      case kSbSystemDeviceTypeBlueRayDiskPlayer:
        youtube_tv_info_->device_type = YouTubeTVInfo::kBlueRayDiskPlayer;
        break;
      case kSbSystemDeviceTypeGameConsole:
        youtube_tv_info_->device_type = YouTubeTVInfo::kGameConsole;
        break;
      case kSbSystemDeviceTypeOverTheTopBox:
        youtube_tv_info_->device_type = YouTubeTVInfo::kOverTheTopBox;
        break;
      case kSbSystemDeviceTypeSetTopBox:
        youtube_tv_info_->device_type = YouTubeTVInfo::kSetTopBox;
        break;
      case kSbSystemDeviceTypeTV:
        youtube_tv_info_->device_type = YouTubeTVInfo::kTV;
        break;
#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
      case kSbSystemDeviceTypeAndroidTV:
        youtube_tv_info_->device_type = YouTubeTVInfo::kAndroidTV;
        break;
#endif  // SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
      case kSbSystemDeviceTypeDesktopPC:
      default:
        youtube_tv_info_->device_type = YouTubeTVInfo::kInvalidDeviceType;
    }

    // Chipset model number
    result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      youtube_tv_info_->chipset_model_number = value;
    }

    // Firmware version
    result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                                 kSystemPropertyMaxLength);
    if (result) {
      youtube_tv_info_->firmware_version = value;
    }

    // Brand
    result = SbSystemGetProperty(kSbSystemPropertyManufacturerName, value,
                                 kSystemPropertyMaxLength);
    SB_DCHECK(result);
    if (result) {
      youtube_tv_info_->brand = value;
    }

    // Model
    result = SbSystemGetProperty(kSbSystemPropertyModelName, value,
                                 kSystemPropertyMaxLength);
    SB_DCHECK(result);
    if (result) {
      youtube_tv_info_->model = value;
    }

    // Connection type
    SbSystemConnectionType connection_type = SbSystemGetConnectionType();
    if (connection_type == kSbSystemConnectionTypeWired) {
      youtube_tv_info_->connection_type = YouTubeTVInfo::kWiredConnection;
    } else if (connection_type == kSbSystemConnectionTypeWireless) {
      youtube_tv_info_->connection_type = YouTubeTVInfo::kWirelessConnection;
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
