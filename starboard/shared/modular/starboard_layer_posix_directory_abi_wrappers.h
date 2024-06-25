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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_DIRECTORY_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_DIRECTORY_ABI_WRAPPERS_H_

#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct musl_dirent {
  uint64_t /* ino_t */ d_ino;
  int64_t /* off_t */ d_off;
  uint16_t /* short */ d_reclen;
  unsigned char d_type;
  char d_name[256];
} musl_dirent;

SB_EXPORT int __abi_wrap_readdir_r(DIR* dirp,
                                   struct musl_dirent* entry,
                                   struct musl_dirent** result);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_DIRECTORY_ABI_WRAPPERS_H_
