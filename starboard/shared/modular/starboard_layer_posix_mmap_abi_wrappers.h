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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_MMAP_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_MMAP_ABI_WRAPPERS_H_

#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"

// The `__abi_wrap_mmap` function converts from the musl's off_t
// type to the platform's off_t which may have different sizes as
// the definition is platform specific.
//
// The wrapper is used by all modular builds, including Evergreen.
//
// For Evergreen-based modular builds, we will rely on the exported_symbols.cc
// mapping logic to map calls to `mmap` to `__abi_wrap_mmap` functions.
//
// For non-Evergreen modular builds, the Cobalt-side shared library will be
// compiled with code that remaps calls to `mmap` to `__abi_wrap_mmap`.

// A matching type for the off_t definition in musl.
typedef int64_t musl_off_t;

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT void* __abi_wrap_mmap(void*, size_t, int, int, int, musl_off_t);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_MMAP_ABI_WRAPPERS_H_
