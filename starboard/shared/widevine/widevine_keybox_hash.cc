// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/widevine/widevine_keybox_hash.h"

#include <sstream>

#include "starboard/common/murmurhash2.h"
#include "third_party/ce_cdm/oemcrypto/mock/src/wv_keybox.h"

namespace wvoec_mock {
namespace {
#if defined(COBALT_WIDEVINE_KEYBOX_INCLUDE)
#include COBALT_WIDEVINE_KEYBOX_INCLUDE
#else  // COBALT_WIDEVINE_KEYBOX_INCLUDE
#error "COBALT_WIDEVINE_KEYBOX_INCLUDE is not defined."
#endif  // COBALT_WIDEVINE_KEYBOX_INCLUDE
}  // namespace
}  // namespace wvoec_mock

namespace starboard {
namespace shared {
namespace widevine {

std::string GetWidevineKeyboxHash() {
  // Note: not a cryptographic hash.
  uint32_t value =
      MurmurHash2_32(reinterpret_cast<const void*>(&wvoec_mock::kKeybox),
                     sizeof(wvoec_mock::WidevineKeybox));
  std::stringstream ss;
  ss << value;
  return ss.str();
}

}  // namespace widevine
}  // namespace shared
}  // namespace starboard
