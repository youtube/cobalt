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
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/cryptography/cryptography_internal.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"
#include "starboard/string.h"

#if SB_API_VERSION < 4
#error "SbCryptography requires SB_API_VERSION >= 4."
#endif

SbCryptographyTransformer SbCryptographyCreateTransformer(
    const char* algorithm,
    int block_size_bits,
    SbCryptographyDirection direction,
    SbCryptographyBlockCipherMode mode,
    const void* initialization_vector,
    int initialization_vector_size,
    const void* key,
    int key_size) {
  if (SbStringCompareAll(algorithm, kSbCryptographyAlgorithmAes) != 0) {
    SB_DLOG(WARNING) << "Unsupported algorithm: " << algorithm;
    return kSbCryptographyInvalidTransformer;
  }

  if (block_size_bits != 128) {
    SB_DLOG(WARNING) << "Unsupported block size: " << block_size_bits;
    return kSbCryptographyInvalidTransformer;
  }

  // TODO: Support 64-bit IV with CTR mode.
  if (initialization_vector_size != block_size_bits / 8) {
    SB_DLOG(WARNING) << "Unsupported initialization_vector_size: "
                     << initialization_vector_size;
    return kSbCryptographyInvalidTransformer;
  }

  // TODO: Support 192, 256-bit keys for AES.
  if (key_size != block_size_bits / 8) {
    SB_DLOG(WARNING) << "Unsupported key_size: " << key_size;
    return kSbCryptographyInvalidTransformer;
  }

  starboard::shared::starboard::cryptography::Algorithm combined_algorithm;
  if (mode == kSbCryptographyBlockCipherModeCbc) {
    combined_algorithm =
        starboard::shared::starboard::cryptography::kAlgorithmAes128Cbc;
  } else if (mode == kSbCryptographyBlockCipherModeCtr) {
    combined_algorithm =
        starboard::shared::starboard::cryptography::kAlgorithmAes128Ctr;
  } else {
    SB_DLOG(WARNING) << "Unsupported block cipher mode: " << mode;
    return kSbCryptographyInvalidTransformer;
  }

  starboard::shared::starboard::cryptography::AES_KEY aeskey = {0};
  int result = -1;
  if (direction == kSbCryptographyDirectionDecode &&
      mode != kSbCryptographyBlockCipherModeCtr) {
    result = AES_set_decrypt_key(key, key_size * 8, &aeskey);
  } else {
    result = AES_set_encrypt_key(key, key_size * 8, &aeskey);
  }

  if (result != 0) {
    SB_DLOG(WARNING) << "Error setting key: " << result;
    return kSbCryptographyInvalidTransformer;
  }

  SbCryptographyTransformer transformer =
      new SbCryptographyTransformerPrivate();
  SbMemorySet(transformer, 0, sizeof(transformer));
  transformer->key = aeskey;
  transformer->algorithm = combined_algorithm;
  transformer->direction = direction;
  SbMemoryCopy(transformer->ivec, initialization_vector,
               initialization_vector_size);
  return transformer;
}
