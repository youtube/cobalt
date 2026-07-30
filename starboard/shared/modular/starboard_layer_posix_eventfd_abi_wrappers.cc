// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_eventfd_abi_wrappers.h"

#include <errno.h>
#include <sys/eventfd.h>

namespace {

int musl_flags_to_platform_flags(int musl_flags) {
  const int known_musl_flags_mask =
      MUSL_EFD_SEMAPHORE | MUSL_EFD_NONBLOCK | MUSL_EFD_CLOEXEC;
  if ((musl_flags & ~known_musl_flags_mask) != 0) {
    return -1;
  }

  int platform_flags = 0;

  if (musl_flags & MUSL_EFD_SEMAPHORE) {
    platform_flags |= EFD_SEMAPHORE;
  }
  if (musl_flags & MUSL_EFD_NONBLOCK) {
    platform_flags |= EFD_NONBLOCK;
  }
  if (musl_flags & MUSL_EFD_CLOEXEC) {
    platform_flags |= EFD_CLOEXEC;
  }

  return platform_flags;
}

}  // namespace

SB_EXPORT int __abi_wrap_eventfd(unsigned int initval, int flags) {
  int platform_flags = musl_flags_to_platform_flags(flags);
  if (platform_flags == -1) {
    errno = EINVAL;
    return -1;
  }

  return eventfd(initval, platform_flags);
}
