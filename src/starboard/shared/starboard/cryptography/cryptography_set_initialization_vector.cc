// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/cryptography.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/cryptography/cryptography_internal.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"

#if SB_API_VERSION < 4
#error "SbCryptography requires SB_API_VERSION >= 4."
#endif

using starboard::shared::starboard::cryptography::AES_gcm128_setiv;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Gcm;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Ctr;

void SbCryptographySetInitializationVector(
    SbCryptographyTransformer transformer,
    const void* initialization_vector,
    int initialization_vector_size) {
  if (transformer->algorithm == kAlgorithmAes128Gcm) {
    AES_gcm128_setiv(&transformer->gcm_context, &transformer->key,
                     initialization_vector, initialization_vector_size);
  } else {
    SbMemoryCopy(transformer->ivec, initialization_vector,
                 std::min(initialization_vector_size,
                          static_cast<int>(sizeof(transformer->ivec))));
  }
}
