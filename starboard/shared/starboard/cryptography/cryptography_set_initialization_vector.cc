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

#include "starboard/configuration.h"
#include "starboard/cryptography.h"
#include "starboard/shared/starboard/cryptography/cryptography_internal.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"

#if SB_API_VERSION < 4
#error "SbCryptography requires SB_API_VERSION >= 4."
#endif

using starboard::shared::starboard::cryptography::AES_gcm128_setiv;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Gcm;

void SbCryptographySetInitializationVector(
    SbCryptographyTransformer transformer,
    const void* initialization_vector,
    int initialization_vector_size) {
  if (transformer->algorithm != kAlgorithmAes128Gcm) {
    SB_DLOG(ERROR) << "Trying to set initialization vector on non-GCM "
                   << "transformer.";
    return;
  }

  AES_gcm128_setiv(&transformer->gcm_context, &transformer->key,
                   initialization_vector, initialization_vector_size);
}
