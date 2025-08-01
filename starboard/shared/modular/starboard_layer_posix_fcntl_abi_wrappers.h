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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_

#include <fcntl.h>
#include <stdarg.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// From //third_party/musl/include/fcntl.h
#define MUSL_F_DUPFD 0
#define MUSL_FD_CLOEXEC 1
#define MUSL_FD_CLOFORK 2
#define MUSL_F_GETFD 3
#define MUSL_F_SETFD 4
#define MUSL_F_GETFL 5
#define MUSL_F_SETFL 6
#define MUSL_F_GETOWN 7
#define MUSL_F_SETOWN 8
#define MUSL_FD_GETOWN_EX 9
#define MUSL_FD_SETOWN_EX 10
#define MUSL_F_GETLK 11
#define MUSL_F_SETLK 12
#define MUSL_F_SETLKW 13
#define MUSL_OFD_GETLK 14
#define MUSL_OFD_SETLK 15
#define MUSL_OFD_SETLKW 16

#define MUSL_F_RDLCK 1
#define MUSL_F_UNLCK 2
#define MUSL_F_WRLCK 3

#define MUSL_O_APPEND 02000
#define MUSL_O_DSYNC 010000
#define MUSL_O_NONBLOCK 04000
#define MUSL_O_RSYNC 04010000
#define MUSL_O_SYNC 04010000
#define MUSL_O_ACCMODE (03 | 010000000)
#define MUSL_O_EXEC 00
#define MUSL_O_RDONLY 01
#define MUSL_O_RDWR 02
#define MUSL_O_SEARCH MUSL_O_EXEC
#define MUSL_O_WRONLY 03

struct musl_flock {
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};

SB_EXPORT int __abi_wrap_fcntl(int fildes, int cmd, ...);
SB_EXPORT int __abi_wrap_fcntl2(int fildes, int cmd);
SB_EXPORT int __abi_wrap_fcntl3(int fildes, int cmd, int arg);
SB_EXPORT int __abi_wrap_fcntl4(int fildes, int cmd, void* arg);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_
