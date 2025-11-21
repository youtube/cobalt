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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_POLL_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_POLL_ABI_WRAPPERS_H_

#include <poll.h>
#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
#define __MUSL_ULONG_TYPE uint64_t
#else
#define __MUSL_ULONG_TYPE uint32_t
#endif

typedef __MUSL_ULONG_TYPE /* unsigned long */ musl_nfds_t;

struct musl_pollfd {
  int32_t /* int */ fd;
  uint16_t /* short */ events;
  uint16_t /* short */ revents;
};

SB_EXPORT int __abi_wrap_poll(struct musl_pollfd*, musl_nfds_t, int32_t);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_POLL_ABI_WRAPPERS_H_
