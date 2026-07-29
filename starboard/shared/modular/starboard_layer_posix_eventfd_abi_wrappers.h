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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_EVENTFD_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_EVENTFD_ABI_WRAPPERS_H_

#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_pipe2_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

// These values are from the musl headers.
#define MUSL_EFD_SEMAPHORE 1
#define MUSL_EFD_NONBLOCK MUSL_O_NONBLOCK
#define MUSL_EFD_CLOEXEC MUSL_O_CLOEXEC

SB_EXPORT int __abi_wrap_eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_EVENTFD_ABI_WRAPPERS_H_
