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
#define MUSL_F_DUPFD_CLOEXEC 1
#define MUSL_F_GETFD 2
#define MUSL_F_SETFD 3
#define MUSL_F_GETFL 4
#define MUSL_F_SETFL 5
#define MUSL_F_GETOWN 6
#define MUSL_F_SETOWN 7
#define MUSL_F_GETLK 8
#define MUSL_F_SETLK 9
#define MUSL_F_SETLKW 10
#define MUSL_FD_CLOEXEC 11
#define MUSL_F_RDLCK 12
#define MUSL_F_UNLCK 13
#define MUSL_F_WRLCK 14

#define MUSL_O_APPEND 02000
#define MUSL_O_DSYNC 010000
#define MUSL_O_NONBLOCK 04000
#define MUSL_O_RSYNC 04010000
#define MUSL_O_SYNC 04010000
#define MUSL_O_PATH 010000000
#define MUSL_O_ACCMODE (03 | 010000000)
#define MUSL_O_RDONLY 00
#define MUSL_O_WRONLY 01
#define MUSL_O_RDWR 02

SB_EXPORT int __abi_wrap_fcntl(int fd, int cmd, va_list args);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_
