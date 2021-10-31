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

using starboard::shared::starboard::cryptography::AES_cbc_encrypt;
using starboard::shared::starboard::cryptography::AES_ctr128_encrypt;
using starboard::shared::starboard::cryptography::AES_decrypt;
using starboard::shared::starboard::cryptography::AES_encrypt;
using starboard::shared::starboard::cryptography::AES_gcm128_decrypt;
using starboard::shared::starboard::cryptography::AES_gcm128_encrypt;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Cbc;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Ctr;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Ecb;
using starboard::shared::starboard::cryptography::kAlgorithmAes128Gcm;

namespace {
inline void* AddPtr(void* pointer, int value) {
  return reinterpret_cast<char*>(pointer) + value;
}

inline const void* AddPtr(const void* pointer, int value) {
  return reinterpret_cast<const char*>(pointer) + value;
}
}  // namespace

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

  switch (transformer->algorithm) {
    case kAlgorithmAes128Cbc:
      AES_cbc_encrypt(in_data, out_data, in_data_size, &(transformer->key),
                      transformer->ivec,
                      transformer->direction == kSbCryptographyDirectionEncode ?
                      SB_AES_ENCRYPT : SB_AES_DECRYPT);
      break;

    case kAlgorithmAes128Ctr:
      AES_ctr128_encrypt(in_data, out_data, in_data_size, &(transformer->key),
                         transformer->ivec, transformer->ecount_buf,
                         &transformer->counter);
      break;

    case kAlgorithmAes128Ecb:
      if (in_data_size % 16 != 0) {
        SB_DLOG(ERROR) << "ECB called with a non-multiple of the block size.";
        return -1;
      }

      if (transformer->direction == kSbCryptographyDirectionEncode) {
        const void* in = in_data;
        void* out = out_data;
        for (int i = 0, blocks = in_data_size / 16; i < blocks; ++i) {
          AES_encrypt(in, out, &transformer->key);
          in = AddPtr(in, 16);
          out = AddPtr(out, 16);
        }
      } else if (transformer->direction == kSbCryptographyDirectionDecode) {
        const void* in = in_data;
        void* out = out_data;
        for (int i = 0, blocks = in_data_size / 16; i < blocks; ++i) {
          AES_decrypt(in, out, &transformer->key);
          in = AddPtr(in, 16);
          out = AddPtr(out, 16);
        }
      } else {
        SB_NOTREACHED();
      }
      break;

    case kAlgorithmAes128Gcm:
      if (transformer->direction == kSbCryptographyDirectionEncode) {
        AES_gcm128_encrypt(&transformer->gcm_context, &transformer->key,
                           in_data, out_data, in_data_size);
      } else if (transformer->direction == kSbCryptographyDirectionDecode) {
        AES_gcm128_decrypt(&transformer->gcm_context, &transformer->key,
                           in_data, out_data, in_data_size);
      } else {
        SB_NOTREACHED();
      }
      break;

    default:
      SB_NOTREACHED();
      return -1;
  }

  return in_data_size;
}

#endif  // SB_API_VERSION < 12
