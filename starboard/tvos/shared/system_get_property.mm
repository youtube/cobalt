// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#import <UIKit/UIKit.h>
#include <sys/utsname.h>

#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info_starboard.h"
#include "starboard/common/device_type.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

#if defined(COBALT_INTERNAL_BUILD)
#include "starboard/keyboxes/tvos/system_properties.h"
#endif

namespace {

constexpr char kPlatformName[] = "Darwin";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > base::saturated_cast<size_t>(value_length)) {
    return false;
  }
  starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || value_length == 0) {
    return false;
  }

  @autoreleasepool {
    switch (property_id) {
      case kSbSystemPropertyBrandName: {
        return CopyStringAndTestIfSuccess(
            out_value, value_length,
            base::starboard::SbSysInfo::Brand().c_str());
      }
      case kSbSystemPropertyFirmwareVersion:
        return CopyStringAndTestIfSuccess(
            out_value, value_length,
            [UIDevice.currentDevice.systemVersion UTF8String]);
      case kSbSystemPropertyModelName: {
        // TODO: Swap the implementation in this case with that in
        // kSbSystemPropertyChipsetModelNumber once player reads the right
        // fields.
        struct utsname systemInfo;
        uname(&systemInfo);
        NSString* deviceType =
            [NSString stringWithCString:systemInfo.machine
                               encoding:NSUTF8StringEncoding];
        deviceType = [deviceType stringByReplacingOccurrencesOfString:@","
                                                           withString:@"-"];

        if (![deviceType containsString:@"AppleTV"]) {
          // This is necessary because Player looks for AppleTV in the UA string
          // and Apple TV simulators only return "x86_64" for the machine.
          deviceType = [NSString stringWithFormat:@"AppleTV%@", deviceType];
        }

        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          [deviceType UTF8String]);
      }
      case kSbSystemPropertyChipsetModelNumber: {
        return CopyStringAndTestIfSuccess(
            out_value, value_length,
            base::starboard::SbSysInfo::ChipsetModelNumber().c_str());
      }
      case kSbSystemPropertyModelYear: {
        return CopyStringAndTestIfSuccess(
            out_value, value_length,
            base::starboard::SbSysInfo::ModelYear().c_str());
      }
      case kSbSystemPropertySystemIntegratorName: {
        return CopyStringAndTestIfSuccess(
            out_value, value_length,
            base::starboard::SbSysInfo::OriginalDesignManufacturer().c_str());
      }
      case kSbSystemPropertySpeechApiKey:
        return false;

      case kSbSystemPropertyUserAgentAuxField: {
        NSString* bundleId = [[NSBundle mainBundle]
            objectForInfoDictionaryKey:@"CFBundleIdentifier"];
        NSString* bundleVersion = [[NSBundle mainBundle]
            objectForInfoDictionaryKey:@"CFBundleVersion"];
        NSString* auxString =
            [NSString stringWithFormat:@"%@/%@", bundleId, bundleVersion];
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          [auxString UTF8String]);
      }

      case kSbSystemPropertyFriendlyName:
        return CopyStringAndTestIfSuccess(
            out_value, value_length, [UIDevice.currentDevice.name UTF8String]);
      case kSbSystemPropertyPlatformName:
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kPlatformName);
      case kSbSystemPropertyCertificationScope:
        // TODO: b/460479616, b/464200271 - Reenable once the expected
        // functionality is clear in Chrobalt.
        SB_NOTIMPLEMENTED();
        return false;
      case kSbSystemPropertyAdvertisingId:
      case kSbSystemPropertyLimitAdTracking: {
        SB_LOG(INFO) << "IFA is not supported via Starboard.";
        return false;
      }
      case kSbSystemPropertyDeviceType:
        return CopyStringAndTestIfSuccess(
            out_value, value_length, starboard::kSystemDeviceTypeOverTheTopBox);
      default:
        SB_NOTIMPLEMENTED();
        return false;
    }
  }
}
