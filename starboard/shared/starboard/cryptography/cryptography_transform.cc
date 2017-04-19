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

int SbCryptographyTransform(SbCryptographyTransformer transformer,
                            const void* in_data,
                            int in_data_size,
                            void* out_data) {
  if (!SbCryptographyIsTransformerValid(transformer) || !in_data || !out_data) {
    return -1;
  }

  if (in_data_size == 0) {
    return 0;
  }

  if (transformer->algorithm ==
      starboard::shared::starboard::cryptography::kAlgorithmAes128Cbc) {
    int enc = transformer->direction == kSbCryptographyDirectionEncode
                  ? SB_AES_ENCRYPT
                  : SB_AES_DECRYPT;
    starboard::shared::starboard::cryptography::AES_cbc_encrypt(
        in_data, out_data, in_data_size, &(transformer->key), transformer->ivec,
        enc);
  } else if (transformer->algorithm ==
             starboard::shared::starboard::cryptography::kAlgorithmAes128Ctr) {
    starboard::shared::starboard::cryptography::AES_ctr128_encrypt(
        in_data, out_data, in_data_size, &(transformer->key), transformer->ivec,
        transformer->ecount_buf, &transformer->counter);
  }

  return in_data_size;
}
