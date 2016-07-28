// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "net/dial/dial_system_config.h"

#include "starboard/system.h"

namespace net {

// static
std::string DialSystemConfig::GetFriendlyName() {
  char buffer[kMaxNameSize];
  SbSystemGetProperty(kSbSystemPropertyFriendlyName, buffer, sizeof(buffer));
  return std::string(buffer);
}

// static
std::string DialSystemConfig::GetManufacturerName() {
  char buffer[kMaxNameSize];
  SbSystemGetProperty(kSbSystemPropertyManufacturerName, buffer,
                      sizeof(buffer));
  return std::string(buffer);
}

// static
std::string DialSystemConfig::GetModelName() {
  char buffer[kMaxNameSize];
  SbSystemGetProperty(kSbSystemPropertyModelName, buffer, sizeof(buffer));
  return std::string(buffer);
}

// static
std::string DialSystemConfig::GeneratePlatformUuid() {
  char buffer[kMaxNameSize];
  SbSystemGetProperty(kSbSystemPropertyPlatformUuid, buffer, sizeof(buffer));
  return std::string(buffer);
}

}  // namespace net
