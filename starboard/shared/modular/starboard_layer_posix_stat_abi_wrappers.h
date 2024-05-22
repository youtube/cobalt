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

// Module Overview: POSIX stat wrappers
//
// These wrappers are used in modular builds (including Evergreen). They
// are needed to handle dealing with differences in platform-specific
// toolchain types and musl-based types. In particular, the `struct stat`
// type may contain differently sized member fields and when the caller is
// Cobalt code compiled with musl it will provide argument data that needs
// to be adjusted when passed to platform-specific system libraries.

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STAT_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STAT_ABI_WRAPPERS_H_

#include <sys/stat.h>  // This should be the headers from the platform toolchain

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

struct musl_stat {
#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
  int64_t /*dev_t*/ st_dev;
  int64_t /*ino_t*/ st_ino;
  int64_t /*nlink_t*/ st_nlink;

  unsigned /*mode_t*/ st_mode;
  unsigned /*uid_t*/ st_uid;
  unsigned /*gid_t*/ st_gid;
  unsigned /*unsigned int*/ __pad0;
  int64_t /*dev_t*/ st_rdev;
  int64_t /*off_t*/ st_size;
  int64_t /*blksize_t*/ st_blksize;
  int64_t /*blkcnt_t*/ st_blocks;

  struct musl_timespec /*struct timespec*/ st_atim;
  struct musl_timespec /*struct timespec*/ st_mtim;
  struct musl_timespec /*struct timespec*/ st_ctim;
  int64_t unused[3];
#else
  uint64_t /*dev_t*/ st_dev;
  uint32_t /*int*/ _st_dev_padding;
  int32_t /*long*/ _st_ino_truncated;

  uint32_t /*mode_t*/ st_mode;
  uint32_t /*nlink_t*/ st_nlink;
  uint32_t /*uid_t*/ st_uid;
  uint32_t /*gid_t*/ st_gid;
  uint64_t /*dev_t*/ st_rdev;
  uint32_t /*int*/ __st_rdev_padding;
  int64_t /*off_t*/ st_size;
  int32_t /*blksize_t*/ st_blksize;
  int64_t /*blkcnt_t*/ st_blocks;
  int32_t _unused[6];
  uint64_t /*ino_t*/ st_ino;
  struct musl_timespec /*struct timespec*/ st_atim;
  struct musl_timespec /*struct timespec*/ st_mtim;
  struct musl_timespec /*struct timespec*/ st_ctim;
#endif
};

SB_EXPORT int __abi_wrap_fstat(int fildes, struct musl_stat* info);

SB_EXPORT int __abi_wrap_stat(const char* path, struct musl_stat* info);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_STAT_ABI_WRAPPERS_H_
