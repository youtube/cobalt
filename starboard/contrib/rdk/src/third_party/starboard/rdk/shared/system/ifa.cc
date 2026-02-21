// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// you may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/extension/ifa.h"
#include "starboard/common/string.h"
#include "third_party/starboard/rdk/shared/system/advertising_id.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace {

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length) {
    return false;
  }
  ::starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

bool GetAdvertisingId(char* out_value, int value_length) {
    std::string prop;
    if (AdvertisingId::GetIfa(prop)) {
      return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
    }
    return false;
}

bool GetLimitAdTracking(char* out_value, int value_length) {
    std::string prop;
    if (AdvertisingId::GetLmtAdTracking(prop)) {
      return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
    }
    return false;
}

const StarboardExtensionIfaApi kIfaApi = {
    kStarboardExtensionIfaName,
    1,  // API version that's implemented.
    &GetAdvertisingId,
    &GetLimitAdTracking,
};

}  // namespace

const void* GetIfaApi() {
  return &kIfaApi;
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
