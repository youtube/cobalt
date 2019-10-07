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

#ifndef STARBOARD_SHARED_WIDEVINE_WIDEVINE_KEYBOX_HASH_H_
#define STARBOARD_SHARED_WIDEVINE_WIDEVINE_KEYBOX_HASH_H_

#include <string>

#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace widevine {

// Computes the checksum of the Widevine Keybox.
// NOTE: this is not a cryptographic hash, but serves our purposes here.
std::string GetWidevineKeyboxHash();

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_WIDEVINE_KEYBOX_HASH_H_
