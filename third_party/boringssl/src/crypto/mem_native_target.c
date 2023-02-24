// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifdef NATIVE_TARGET_BUILD
// Note: the definitions below are mostly copied from crypto/internal.h,
// which was removed in an ealier change.

#include <openssl/mem.h>

#include <string.h>

// Language bug workarounds.
//
// Most C standard library functions are undefined if passed NULL, even when the
// corresponding length is zero. This gives them (and, in turn, all functions
// which call them) surprising behavior on empty arrays. Some compilers will
// miscompile code due to this rule. See also
// https://www.imperialviolet.org/2016/06/26/nonnull.html
//
// These wrapper functions behave the same as the corresponding C standard
// functions, but behave as expected when passed NULL if the length is zero.
//
// Note |OPENSSL_memcmp| is a different function from |CRYPTO_memcmp|.

// C++ defines |memchr| as a const-correct overload.

const void *OPENSSL_memchr(const void *s, int c, size_t n) {
  if (n == 0) {
    return NULL;
  }

  return memchr(s, c, n);
}

int OPENSSL_memcmp(const void *s1, const void *s2, size_t n) {
  if (n == 0) {
    return 0;
  }

  return memcmp(s1, s2, n);
}

void *OPENSSL_memcpy(void *dst, const void *src, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memcpy(dst, src, n);
}

void *OPENSSL_memmove(void *dst, const void *src, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memmove(dst, src, n);
}

void *OPENSSL_memset(void *dst, int c, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memset(dst, c, n);
}
#endif  // NATIVE_TARGET_BUILD
