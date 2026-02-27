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
#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_unistd_abi_wrappers.h"

static_assert(O_CREAT == 0100,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_APPEND == 02000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_DSYNC == 010000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_NONBLOCK == 04000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_RSYNC == 04010000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_SYNC == 04010000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(O_ASYNC == 020000,
              "The Starboard layer wrapper expects this value from musl");
#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
static_assert(FASYNC == O_ASYNC,
              "The Starboard layer wrapper expects this value from musl");
static_assert(FNONBLOCK == O_NONBLOCK,
              "The Starboard layer wrapper expects this value from musl");
static_assert(FNDELAY == O_NDELAY,
              "The Starboard layer wrapper expects this value from musl");
#endif

#ifdef __cplusplus
extern "C" {
#endif

// From //third_party/musl/include/fcntl.h
#define MUSL_F_DUPFD 0
#define MUSL_F_DUPFD_CLOEXEC 1030
#define MUSL_F_GETFD 1
#define MUSL_F_SETFD 2
#define MUSL_F_GETFL 3
#define MUSL_F_SETFL 4
#define MUSL_F_GETLK 5
#define MUSL_F_SETLK 6
#define MUSL_F_SETLKW 7
#define MUSL_F_SETOWN 8
#define MUSL_F_GETOWN 9
#define MUSL_F_SETOWN_EX 15
#define MUSL_F_GETOWN_EX 16
#if defined(_LARGEFILE64_SOURCE)
#define MUSL_F_SETLK64 MUSL_F_SETLK
#define MUSL_F_SETLKW64 MUSL_F_SETLKW
#endif
#define MUSL_F_OFD_GETLK 36
#define MUSL_F_OFD_SETLK 37
#define MUSL_F_OFD_SETLKW 38

#define MUSL_FD_CLOEXEC 1

#define MUSL_F_RDLCK 0
#define MUSL_F_WRLCK 1
#define MUSL_F_UNLCK 2

#define MUSL_O_APPEND 02000
#define MUSL_O_DSYNC 010000
#define MUSL_O_NONBLOCK 04000
#define MUSL_O_RSYNC 04010000
#define MUSL_O_SYNC 04010000
#define MUSL_O_PATH 010000000
#define MUSL_O_ACCMODE (03 | MUSL_O_PATH)
#define MUSL_O_RDONLY 00
#define MUSL_O_WRONLY 01
#define MUSL_O_RDWR 02

#define MUSL_SEEK_SET 0
#define MUSL_SEEK_CUR 1
#define MUSL_SEEK_END 2

struct musl_flock {
  short l_type;
  short l_whence;
  musl_off_t l_start;
  musl_off_t l_len;
  musl_pid_t l_pid;
};

// Musl O_ mode constants
#define MUSL_O_SEARCH 010000000
#define MUSL_O_EXEC 010000000
#define MUSL_O_RDONLY 00
#define MUSL_O_WRONLY 01
#define MUSL_O_RDWR 02

#define MUSL_AT_REMOVEDIR 0x200

SB_EXPORT int __abi_wrap_fcntl(int fildes, int cmd, ...);

SB_EXPORT int __abi_wrap_openat(int fildes, const char* path, int oflag, ...);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_
