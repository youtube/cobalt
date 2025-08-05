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
#define MUSL_F_DUPFD 2000
#define MUSL_F_DUPFD_CLOEXEC 1030
#define MUSL_F_DUPFD_CLOFORK 2001
#define MUSL_F_GETFD 2002
#define MUSL_F_SETFD 2003
#define MUSL_F_GETFL 2004
#define MUSL_F_SETFL 2005
#define MUSL_F_GETOWN 2006
#define MUSL_F_SETOWN 2007
#define FD_GETOWN_EX 2008
#define FD_SETOWN_EX 2009
#define MUSL_F_GETLK 2010
#define MUSL_F_SETLK 2011
#define MUSL_F_SETLKW 2012
#if defined(_LARGEFILE64_SOURCE)
#define MUSL_F_SETLK64 MUSL_F_SETLK
#define MUSL_F_SETLKW64 MUSL_F_SETLKW
#endif
#define MUSL_F_OFD_GETLK 2013
#define MUSL_F_OFD_SETLK 2014
#define MUSL_F_OFD_SETLKW 2015
#define MUSL_FD_CLOEXEC 1

#define MUSL_F_RDLCK 0
#define MUSL_F_UNLCK 1
#define MUSL_F_WRLCK 2

#define MUSL_O_APPEND 02000
#define MUSL_O_DSYNC 010000
#define MUSL_O_NONBLOCK 04000
#define MUSL_O_RSYNC 04010000
#define MUSL_O_SYNC 04010000
#define MUSL_O_PATH 010000000
#define MUSL_O_ACCMODE (03 | MUSL_O_PATH)
#define MUSL_O_EXEC MUSL_O_PATH
#define MUSL_O_SEARCH MUSL_O_PATH
#define MUSL_O_RDONLY 00
#define MUSL_O_WRONLY 01
#define MUSL_O_RDWR 02

struct muslflock {
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};

SB_EXPORT int __abi_wrap_fcntl(int fildes, int cmd, ...);
SB_EXPORT int Fcntl(int fildes, int cmd);
SB_EXPORT int FcntlInt(int fildes, int cmd, int arg);
SB_EXPORT int FcntlPtr(int fildes, int cmd, void* arg);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_FCNTL_ABI_WRAPPERS_H_
