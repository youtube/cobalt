// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Based on refcount_c11.c implementation.

#include "internal.h"

#if defined(STARBOARD)

#include "starboard/atomic.h"

void CRYPTO_refcount_inc(CRYPTO_refcount_t *count) {
  CRYPTO_refcount_t expected = SbAtomicNoBarrier_Load(count);

  while (expected != CRYPTO_REFCOUNT_MAX) {
    CRYPTO_refcount_t new_value = expected + 1;
    CRYPTO_refcount_t old_value = SbAtomicNoBarrier_CompareAndSwap(
        count, expected, new_value);
    if (old_value == expected) {
      break;
    }
    expected = old_value;
  }
}

int CRYPTO_refcount_dec_and_test_zero(CRYPTO_refcount_t *count) {
  CRYPTO_refcount_t expected = SbAtomicNoBarrier_Load(count);

  for (;;) {
    if (expected == 0) {
      OPENSSL_port_abort();
    } else if (expected == CRYPTO_REFCOUNT_MAX) {
      return 0;
    } else {
      CRYPTO_refcount_t new_value = expected - 1;
      CRYPTO_refcount_t old_value = SbAtomicNoBarrier_CompareAndSwap(
          count, expected, new_value);
      if (old_value == expected) {
        return new_value == 0;
      }
      expected = old_value;
    }
  }
}

#endif  // defined(STARBOARD)
