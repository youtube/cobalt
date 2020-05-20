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

#include "starboard/configuration.h"

#if SB_API_VERSION < 12

#include "starboard/cryptography.h"
#include "starboard/shared/starboard/cryptography/cryptography_internal.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"

using starboard::shared::starboard::cryptography::AES_gcm128_aad;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Gcm;

bool SbCryptographySetAuthenticatedData(
    SbCryptographyTransformer transformer,
    const void* data,
    int data_size) {
  if (transformer->algorithm != kAlgorithmAes128Gcm) {
    SB_DLOG(ERROR) << "Trying to set AAD on non-GCM transformer.";
    return false;
  }

  int result = AES_gcm128_aad(&transformer->gcm_context, data, data_size);
  return result == 1;
}

#endif  // SB_API_VERSION < 12
