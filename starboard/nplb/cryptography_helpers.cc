/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/cryptography_helpers.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {
int FromHex(uint8_t* out, char in) {
  if (in >= '0' && in <= '9') {
    *out = in - '0';
    return 1;
  }
  if (in >= 'a' && in <= 'f') {
    *out = in - 'a' + 10;
    return 1;
  }
  if (in >= 'A' && in <= 'F') {
    *out = in - 'A' + 10;
    return 1;
  }

  return 0;
}
}  // namespace

void DecodeHex(scoped_array<uint8_t>* out,
               int* out_len,
               const char* in,
               int test_num,
               const char* description) {
  if (in == NULL) {
    out->reset();
    *out_len = 0;
    return;
  }

  size_t len = strlen(in);
  if (len & 1) {
    ADD_FAILURE() << description << ": Odd length.";
    return;
  }

  scoped_array<uint8_t> buf(new uint8_t[len / 2]);
  if (!buf) {
    ADD_FAILURE() << description << ": Memory fail.";
    return;
  }

  for (size_t i = 0; i < len; i += 2) {
    uint8_t v, v2;
    bool result = FromHex(&v, in[i]) && FromHex(&v2, in[i + 1]);
    if (!result) {
      ADD_FAILURE() << description << ": Invalid character at " << i << ".";
      continue;
    }

    buf[i / 2] = (v << 4) | v2;
  }

  *out = buf.Pass();
  *out_len = static_cast<int>(len / 2);
}

std::string HexDump(const void* in, int len) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(in);

  std::string result;
  for (size_t i = 0; i < len; i++) {
    char hex[3] = {0};
    SbStringFormatF(hex, 3, "%02x", data[i]);
    result += hex;
  }

  return result;
}

}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 12
