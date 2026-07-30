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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STATVFS_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STATVFS_ABI_WRAPPERS_H_

#include <stdint.h>
#include <stdio.h>
#include <sys/statvfs.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// musl's fsblkcnt_t/fsfilcnt_t are 64-bit on every arch but bionic defines them
// as 32-bit `unsigned long` on 32-bit targets. Use fixed-width 64-bit types so
// this struct matches musl's layout.
typedef uint64_t musl_fsblkcnt_t;
typedef uint64_t musl_fsfilcnt_t;

struct musl_statvfs {
  unsigned long f_bsize;
  unsigned long f_frsize;
  musl_fsblkcnt_t f_blocks;
  musl_fsblkcnt_t f_bfree;
  musl_fsblkcnt_t f_bavail;
  musl_fsfilcnt_t f_files;
  musl_fsfilcnt_t f_ffree;
  musl_fsfilcnt_t f_favail;
  unsigned long f_fsid;
  unsigned : 8 * (2 * sizeof(int) - sizeof(long));
  unsigned long f_flag;
  unsigned long f_namemax;
  int __f_spare[6];
};

SB_EXPORT int __abi_wrap_statvfs(const char* path, struct musl_statvfs* buf);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STATVFS_ABI_WRAPPERS_H_
