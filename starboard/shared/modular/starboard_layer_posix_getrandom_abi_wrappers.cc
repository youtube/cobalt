// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_getrandom_abi_wrappers.h"

#include <errno.h>
#include <sys/random.h>

ssize_t __abi_wrap_getrandom(void* buf, size_t buflen, unsigned flags) {
  unsigned platform_flags = 0;
  if (flags & MUSL_GRND_NONBLOCK) {
    platform_flags |= GRND_NONBLOCK;
    flags &= ~MUSL_GRND_NONBLOCK;
  }
  if (flags & MUSL_GRND_RANDOM) {
    platform_flags |= GRND_RANDOM;
    flags &= ~MUSL_GRND_RANDOM;
  }
  if (flags) {
    errno = EINVAL;
    return -1;
  }

  return getrandom(buf, buflen, platform_flags);
}
