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

#import <AdSupport/ASIdentifierManager.h>
#import <AppTrackingTransparency/ATTrackingManager.h>
#import <UIKit/UIKit.h>
#include <sys/utsname.h>

#include "starboard/common/device_type.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

#if defined(INTERNAL_BUILD)
#include "starboard/keyboxes/tvos/system_properties.h"
#endif

namespace {
struct AppleTVDevice {
  const char* identifier;
  const char* chipset;
  const char* year;
};

// Array of chipset and model release year for known Apple TV devices.
struct AppleTVDevice kDevices[] = {
    {"AppleTV1,1", "Intel Pentium M", "2007"},
    {"AppleTV2,1", "Apple A4", "2010"},
    {"AppleTV3,1", "Apple A5", "2012"},
    {"AppleTV3,2", "Apple A5", "2013"},
    {"AppleTV5,3", "Apple A8", "2015"},
    {"AppleTV6,2", "Apple A10X Fusion", "2017"},
    {"AppleTV11,1", "Apple A12 Bionic", "2021"},
    {"AppleTV14,1", "Apple A15 Bionic", "2022"},
};

// Year for unknown Apple TV devices. This assumes that they will be as
// capable as the most recent known device.
const char kUnknownModelYear[] = "2022";
const char kUnknownChipset[] = "ChipsetUnknown";

const char* kBrandName = "Apple";
const char* kPlatformName = "Darwin";
const char kSystemIntegrator[] = "YouTube";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length) {
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
      case kSbSystemPropertyBrandName:
        return CopyStringAndTestIfSuccess(out_value, value_length, kBrandName);
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
        struct utsname systemInfo;
        uname(&systemInfo);
        for (auto& device : kDevices) {
          if (!strcasecmp(device.identifier, systemInfo.machine)) {
            return CopyStringAndTestIfSuccess(out_value, value_length,
                                              device.chipset);
          }
        }
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kUnknownChipset);
      }
      case kSbSystemPropertyModelYear: {
        struct utsname systemInfo;
        uname(&systemInfo);
        for (auto& device : kDevices) {
          if (!strcasecmp(device.identifier, systemInfo.machine)) {
            return CopyStringAndTestIfSuccess(out_value, value_length,
                                              device.year);
          }
        }
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kUnknownModelYear);
      }
      case kSbSystemPropertySystemIntegratorName:
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kSystemIntegrator);
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

      case kSbSystemPropertyAdvertisingId: {
        NSString* advertisingId = [[ASIdentifierManager sharedManager]
                                       .advertisingIdentifier UUIDString];
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          [advertisingId UTF8String]);
      }
      case kSbSystemPropertyLimitAdTracking: {
        // The assumption is we will limit ad tracking if the status from OS is
        // not explicitly authorized (including if the API is not present on
        // older OS versions).
        bool limitAdTracking = true;
        if (@available(tvOS 14.0, *)) {
          ATTrackingManagerAuthorizationStatus status =
              [ATTrackingManager trackingAuthorizationStatus];
          if (status == ATTrackingManagerAuthorizationStatusAuthorized) {
            limitAdTracking = false;
          }
        }
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          limitAdTracking ? "1" : "0");
      }

      case kSbSystemPropertyDeviceType:
        return CopyStringAndTestIfSuccess(
            out_value, value_length, starboard::kSystemDeviceTypeOverTheTopBox);
    }
  }

  return false;
}
