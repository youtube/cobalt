// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <poll.h>
#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t /* unsigned long */ musl_nfds_t;

struct musl_pollfd {
  int32_t /* int */ fd;
  uint16_t /* short */ events;
  uint16_t /* short */ revents;
};

SB_EXPORT int __abi_wrap_poll(struct musl_pollfd*, musl_nfds_t, int);

#ifdef __cplusplus
}  // extern "C"
#endif
