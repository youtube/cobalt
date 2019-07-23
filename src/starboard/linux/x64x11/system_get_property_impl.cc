// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/linux/x64x11/system_get_property_impl.h"

#include <netdb.h>
#include <linux/if.h>  // NOLINT(build/include_alpha)
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/x64x11/system_properties.h"

namespace {

const char* kFriendlyName = "Linux Desktop";
const char* kPlatformName = "X11; Linux x86_64";

#if SB_API_VERSION < 10

bool GetPlatformUuid(char* out_value, int value_length) {
  struct ifreq interface;
  struct ifconf config;
  char buf[1024];

  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (fd == -1) {
    return false;
  }
  config.ifc_len = sizeof(buf);
  config.ifc_buf = buf;
  int result = ioctl(fd, SIOCGIFCONF, &config);
  if (result == -1) {
    return false;
  }

  struct ifreq* cur_interface = config.ifc_req;
  const struct ifreq* const end = cur_interface +
      (config.ifc_len / sizeof(struct ifreq));

  for (; cur_interface != end; ++cur_interface) {
    SbStringCopy(interface.ifr_name,
        cur_interface->ifr_name,
        sizeof(cur_interface->ifr_name));
    if (ioctl(fd, SIOCGIFFLAGS, &interface) == -1) {
      continue;
    }
    if (interface.ifr_flags & IFF_LOOPBACK) {
      continue;
    }
    if (ioctl(fd, SIOCGIFHWADDR, &interface) == -1) {
      continue;
    }
    SbStringFormatF(out_value, value_length, "%x:%x:%x:%x:%x:%x",
        interface.ifr_addr.sa_data[0], interface.ifr_addr.sa_data[1],
        interface.ifr_addr.sa_data[2], interface.ifr_addr.sa_data[3],
        interface.ifr_addr.sa_data[4], interface.ifr_addr.sa_data[5]);
    return true;
  }
  return false;
}

#endif  // SB_API_VERSION < 10

}  // namespace

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace x64x11 {

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (SbStringGetLength(from_value) + 1 > value_length)
    return false;
  SbStringCopy(out_value, from_value, value_length);
  return true;
}

bool GetSystemProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  switch (property_id) {
    case kSbSystemPropertyBrandName:
    case kSbSystemPropertyChipsetModelNumber:
    case kSbSystemPropertyFirmwareVersion:
    case kSbSystemPropertyModelName:
    case kSbSystemPropertyModelYear:
#if SB_API_VERSION >= 11
    case kSbSystemPropertyOriginalDesignManufacturerName:
#else
    case kSbSystemPropertyNetworkOperatorName:
#endif
    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

#if SB_API_VERSION < 10
    case kSbSystemPropertyPlatformUuid:
      return GetPlatformUuid(out_value, value_length);
#endif  // SB_API_VERSION < 10

#if SB_API_VERSION >= 11
    case kSbSystemPropertyCertificationScope:
      if (kCertificationScope[0] == '\0')
        return false;
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kCertificationScope);

    case kSbSystemPropertyBase64EncodedCertificationSecret:
      if (kBase64EncodedCertificationSecret[0] == '\0')
        return false;
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kBase64EncodedCertificationSecret);
#endif  // SB_API_VERSION >= 11

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}

}  // namespace x64x11
}  // namespace starboard
