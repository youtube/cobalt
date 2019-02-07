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

#include <openssl/rand.h>

#if defined(STARBOARD) && !defined(BORINGSSL_UNSAFE_DETERMINISTIC_MODE)

#include <openssl/opensslconf.h>
#if !defined(OPENSSL_SYS_STARBOARD)
#include <string.h>
#endif  // !defined(OPENSSL_SYS_STARBOARD)
#include <openssl/chacha.h>
#include <openssl/mem.h>

#include "../fipsmodule/rand/internal.h"
#include "../internal.h"

void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  SbSystemGetRandomData(out, requested);
}

#endif  // BORINGSSL_UNSAFE_DETERMINISTIC_MODE
