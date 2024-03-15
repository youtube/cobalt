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

#ifndef STARBOARD_SHARED_MODULAR_POSIX_STAT_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_POSIX_STAT_WRAPPERS_H_

#include <sys/stat.h>  // This should be the headers from the platform toolchain

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/posix_time_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

struct musl_stat {
  unsigned /*dev_t*/ st_dev;
  unsigned /*ino_t*/ st_ino;
  unsigned /*nlink_t*/ st_nlink;

  unsigned /*mode_t*/ st_mode;
  unsigned /*uid_t*/ st_uid;
  unsigned /*gid_t*/ st_gid;
  unsigned /*unsigned int*/ __pad0;
  unsigned /*dev_t*/ st_rdev;
  int64_t /*off_t*/ st_size;
  __MUSL_LONG_TYPE /*blksize_t*/ st_blksize;
  int64_t /*blkcnt_t*/ st_blocks;

  musl_timespec /*struct timespec*/ st_atim;
  musl_timespec /*struct timespec*/ st_mtim;
  musl_timespec /*struct timespec*/ st_ctim;
  int64_t __unused[3];
};

SB_EXPORT int __wrap_stat(const char* path, struct musl_stat* info);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_POSIX_STAT_WRAPPERS_H_
