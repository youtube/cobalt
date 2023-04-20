// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard System Property module
//
// Implements helper functions for getting system property.

#ifndef STARBOARD_COMMON_SYSTEM_PROPERTY_H_
#define STARBOARD_COMMON_SYSTEM_PROPERTY_H_

#include <string>

#include "starboard/system.h"

namespace starboard {

const size_t kSystemPropertyMaxLength = 1024;

inline std::string GetSystemPropertyString(SbSystemPropertyId id) {
  char value[kSystemPropertyMaxLength];
  bool result;
  result = SbSystemGetProperty(id, value, kSystemPropertyMaxLength);
  if (result) {
    return std::string(value);
  }
  return "";
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_SYSTEM_PROPERTY_H_
