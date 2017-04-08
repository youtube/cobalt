/* ====================================================================
 * Copyright (c) 2002-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ==================================================================== */

// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_SOFTWARE_AES_H_
#define STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_SOFTWARE_AES_H_

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace cryptography {

/* Raw AES functions. */
#define SB_AES_ENCRYPT 1
#define SB_AES_DECRYPT 0

/* AES_MAXNR is the maximum number of AES rounds. */
#define SB_AES_MAXNR 14
#define SB_AES_BLOCK_SIZE 16

/* aes_key_st should be an opaque type, but EVP requires that the size be
 * known. */
struct aes_key_st {
  uint32_t rd_key[4 * (SB_AES_MAXNR + 1)];
  unsigned rounds;
};

typedef struct aes_key_st AES_KEY;

/* AES_set_encrypt_key configures |aeskey| to encrypt with the |bits|-bit key,
 * |key|.
 *
 * WARNING: unlike other OpenSSL functions, this returns zero on success and a
 * negative number on error. */
int AES_set_encrypt_key(const void* key, unsigned bits, AES_KEY* aeskey);

/* AES_set_decrypt_key configures |aeskey| to decrypt with the |bits|-bit key,
 * |key|.
 *
 * WARNING: unlike other OpenSSL functions, this returns zero on success and a
 * negative number on error. */
int AES_set_decrypt_key(const void* key, unsigned bits, AES_KEY* aeskey);

/* AES_encrypt encrypts a single block from |in| to |out| with |key|. The |in|
 * and |out| pointers may overlap. */
void AES_encrypt(const void* in, void* out, const AES_KEY* key);

/* AES_decrypt decrypts a single block from |in| to |out| with |key|. The |in|
 * and |out| pointers may overlap. */
void AES_decrypt(const void* in, void* out, const AES_KEY* key);

/* Block cipher modes. */
/* AES_ctr128_encrypt encrypts (or decrypts, it's the same in CTR mode) |len|
 * bytes from |in| to |out|. The |num| parameter must be set to zero on the
 * first call and |ivec| will be incremented. */
void AES_ctr128_encrypt(const void* in,
                        void* out,
                        size_t len,
                        const AES_KEY* key,
                        uint8_t ivec[SB_AES_BLOCK_SIZE],
                        uint8_t ecount_buf[SB_AES_BLOCK_SIZE],
                        uint32_t* num);

/* AES_cbc_encrypt encrypts (or decrypts, if |enc| == |AES_DECRYPT|) |len|
 * bytes from |in| to |out|. The length must be a multiple of the block size. */
void AES_cbc_encrypt(const void* in,
                     void* out,
                     size_t len,
                     const AES_KEY* key,
                     uint8_t* ivec,
                     const int enc);

}  // namespace cryptography
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_CRYPTOGRAPHY_SOFTWARE_AES_H_
