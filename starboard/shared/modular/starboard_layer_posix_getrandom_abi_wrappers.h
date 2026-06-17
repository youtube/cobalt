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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_GETRANDOM_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_GETRANDOM_ABI_WRAPPERS_H_

#include <sys/random.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Values for `flags`
#define MUSL_GRND_NONBLOCK 0x0001
#define MUSL_GRND_RANDOM 0x0002

SB_EXPORT ssize_t __abi_wrap_getrandom(void* buf,
                                       size_t buflen,
                                       unsigned flags);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_GETRANDOM_ABI_WRAPPERS_H_
