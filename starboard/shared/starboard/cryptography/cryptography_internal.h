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

#ifndef STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_CRYPTOGRAPHY_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_CRYPTOGRAPHY_INTERNAL_H_

#if SB_API_VERSION >= 12
#error "Starboard Crypto API is deprecated"
#else

#include "starboard/cryptography.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace cryptography {

// The modes supported by this software-only implementation.
enum Algorithm {
  kAlgorithmAes128Cbc,
  kAlgorithmAes128Ctr,
  kAlgorithmAes128Ecb,
  kAlgorithmAes128Gcm,
};

}  // namespace cryptography
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

struct SbCryptographyTransformerPrivate {
  starboard::shared::starboard::cryptography::Algorithm algorithm;
  SbCryptographyDirection direction;
  starboard::shared::starboard::cryptography::GCM128_CONTEXT gcm_context;
  starboard::shared::starboard::cryptography::AES_KEY key;
  uint8_t ivec[SB_AES_BLOCK_SIZE];
  uint8_t ecount_buf[SB_AES_BLOCK_SIZE];
  uint32_t counter;
};

#endif  // SB_API_VERSION >= 12

#endif  // STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_CRYPTOGRAPHY_INTERNAL_H_
