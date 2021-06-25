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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/cryptography.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/cryptography/cryptography_internal.h"
#include "starboard/shared/starboard/cryptography/software_aes.h"

using starboard::shared::starboard::cryptography::AES_KEY;
using starboard::shared::starboard::cryptography::AES_gcm128_init;
using starboard::shared::starboard::cryptography::Algorithm;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Cbc;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Ctr;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Ecb;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Gcm;

SbCryptographyTransformer SbCryptographyCreateTransformer(
    const char* algorithm,
    int block_size_bits,
    SbCryptographyDirection direction,
    SbCryptographyBlockCipherMode mode,
    const void* initialization_vector,
    int initialization_vector_size,
    const void* key,
    int key_size) {
  if (strcmp(algorithm, kSbCryptographyAlgorithmAes) != 0) {
    SB_DLOG(WARNING) << "Unsupported algorithm: " << algorithm;
    return kSbCryptographyInvalidTransformer;
  }

  if (block_size_bits != 128) {
    SB_DLOG(WARNING) << "Unsupported block size: " << block_size_bits;
    return kSbCryptographyInvalidTransformer;
  }

  Algorithm combined_algorithm;
  if (mode == kSbCryptographyBlockCipherModeCbc) {
    combined_algorithm = kAlgorithmAes128Cbc;
  } else if (mode == kSbCryptographyBlockCipherModeCtr) {
    combined_algorithm = kAlgorithmAes128Ctr;
  } else if (mode == kSbCryptographyBlockCipherModeEcb) {
    combined_algorithm = kAlgorithmAes128Ecb;
  } else if (mode == kSbCryptographyBlockCipherModeGcm) {
    combined_algorithm = kAlgorithmAes128Gcm;
  } else {
    SB_DLOG(WARNING) << "Unsupported block cipher mode: " << mode;
    return kSbCryptographyInvalidTransformer;
  }

  if (mode == kSbCryptographyBlockCipherModeGcm ||
      mode == kSbCryptographyBlockCipherModeEcb) {
    if (initialization_vector_size != 0) {
      SB_DLOG(WARNING) << "Unsupported initialization_vector_size: "
                       << initialization_vector_size;
      return kSbCryptographyInvalidTransformer;
    }
  } else if (mode == kSbCryptographyBlockCipherModeCtr) {
    if (initialization_vector_size != 0 && initialization_vector_size != 12 &&
        initialization_vector_size != 16 && initialization_vector_size != 32) {
      SB_DLOG(WARNING) << "Unsupported CTR initialization_vector_size: "
                       << initialization_vector_size;
      return kSbCryptographyInvalidTransformer;
    }
  } else if (initialization_vector_size != block_size_bits / 8) {
    SB_DLOG(WARNING) << "Unsupported initialization_vector_size: "
                     << initialization_vector_size;
    return kSbCryptographyInvalidTransformer;
  }

  if (key_size != 16 && key_size != 24 && key_size != 32) {
    SB_DLOG(WARNING) << "Unsupported key_size: " << key_size;
    return kSbCryptographyInvalidTransformer;
  }

  AES_KEY aeskey = {0};
  int result = -1;
  if (direction == kSbCryptographyDirectionDecode &&
      mode != kSbCryptographyBlockCipherModeCtr &&
      mode != kSbCryptographyBlockCipherModeGcm) {
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
  memset(transformer, 0, sizeof(transformer));
  transformer->key = aeskey;
  transformer->algorithm = combined_algorithm;
  transformer->direction = direction;
  if (initialization_vector_size) {
    memcpy(transformer->ivec, initialization_vector,
                 initialization_vector_size);
  }

  if (transformer->algorithm == kAlgorithmAes128Gcm) {
    AES_gcm128_init(&transformer->gcm_context, &transformer->key,
                    direction == kSbCryptographyDirectionEncode ? 1 : 0);
  }

  return transformer;
}

#endif  // SB_API_VERSION < 12
