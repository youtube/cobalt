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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UIO_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UIO_ABI_WRAPPERS_H_

#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_unistd_abi_wrappers.h"

struct musl_iovec {
  void* iov_base;
  size_t iov_len;
};

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT ssize_t __abi_wrap_readv(int fildes,
                                   const struct musl_iovec* iov,
                                   int iovcnt);

SB_EXPORT ssize_t __abi_wrap_writev(int fildes,
                                    const struct musl_iovec* iov,
                                    int iovcnt);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UIO_ABI_WRAPPERS_H_
