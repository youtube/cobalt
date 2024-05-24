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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_

#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"

// The `__abi_wrap_lseek` function converts from the musl's off_t
// type to the platform's off_t which may have different sizes as
// the definition is platform specific.
// The `__abi_wrap_read` function converts ssize_t to int32_t or
// int64_t depending on the platform.
//
// The wrapper is used by all modular builds, including Evergreen.
//
// For Evergreen-based modular builds, we will rely on the exported_symbols.cc
// mapping logic to map calls to file IO functions to `__abi_wrap_` file IO
// functions.
//
// For non-Evergreen modular builds, the Cobalt-side shared library will be
// compiled with code that remaps calls to file IO functions to `__abi_wrap_`
// file IO functions.

// A matching type for the off_t definition in musl.

typedef int64_t musl_off_t;

#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT int __abi_wrap_ftruncate(int fildes, musl_off_t length);

SB_EXPORT musl_off_t __abi_wrap_lseek(int fildes,
                                      musl_off_t offset,
                                      int whence);

SB_EXPORT ssize_t __abi_wrap_read(int fildes, void* buf, size_t nbyte);

SB_EXPORT ssize_t __abi_wrap_write(int fildes, const void* buf, size_t nbyte);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNISTD_ABI_WRAPPERS_H_
