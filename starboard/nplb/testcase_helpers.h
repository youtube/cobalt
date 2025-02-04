// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_TESTCASE_HELPERS_H_
#define STARBOARD_NPLB_TESTCASE_HELPERS_H_

#include <string.h>
#include "starboard/system.h"
#include "starboard/common/log.h"

namespace starboard {
namespace nplb {

typedef enum PlatformType {
    kPlatformTypeLinux,
    kPlatformTypeAndroid,
    kPlatformTypeUnknown
} PlatformType;

static PlatformType runtimePlatformType = PlatformType::kPlatformTypeUnknown;

static inline PlatformType GetRuntimePlatform() {
  if (runtimePlatformType != PlatformType::kPlatformTypeUnknown) {
      return runtimePlatformType;
  }

  // Set default platform type to Linux.
  runtimePlatformType = PlatformType::kPlatformTypeLinux;

  // Get runtime platform type from starboard.
  const int bufSize = 256;
  char platformStrBuf[bufSize];
  SbSystemGetProperty(kSbSystemPropertyPlatformName, platformStrBuf, bufSize);

  if (std::string(platformStrBuf).find("Android") != std::string::npos) {
    runtimePlatformType = PlatformType::kPlatformTypeAndroid;
  }
  return runtimePlatformType;
}

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_TESTCASE_HELPERS_H_
