// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_INOTIFY_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_INOTIFY_ABI_WRAPPERS_H_

#include <stdint.h>

#include "starboard/shared/modular/starboard_layer_posix_pipe2_abi_wrappers.h"  // MUSL_O_CLOEXEC, MUSL_O_NONBLOCK

#ifdef __cplusplus
extern "C" {
#endif

#define MUSL_IN_CLOEXEC MUSL_O_CLOEXEC
#define MUSL_IN_NONBLOCK MUSL_O_NONBLOCK

#define MUSL_IN_ACCESS 0x00000001
#define MUSL_IN_MODIFY 0x00000002
#define MUSL_IN_ATTRIB 0x00000004
#define MUSL_IN_CLOSE_WRITE 0x00000008
#define MUSL_IN_CLOSE_NOWRITE 0x00000010
#define MUSL_IN_CLOSE (MUSL_IN_CLOSE_WRITE | MUSL_IN_CLOSE_NOWRITE)
#define MUSL_IN_OPEN 0x00000020
#define MUSL_IN_MOVED_FROM 0x00000040
#define MUSL_IN_MOVED_TO 0x00000080
#define MUSL_IN_MOVE (MUSL_IN_MOVED_FROM | MUSL_IN_MOVED_TO)
#define MUSL_IN_CREATE 0x00000100
#define MUSL_IN_DELETE 0x00000200
#define MUSL_IN_DELETE_SELF 0x00000400
#define MUSL_IN_MOVE_SELF 0x00000800
#define MUSL_IN_ALL_EVENTS 0x00000fff

#define MUSL_IN_UNMOUNT 0x00002000
#define MUSL_IN_Q_OVERFLOW 0x00004000
#define MUSL_IN_IGNORED 0x00008000

#define MUSL_IN_ONLYDIR 0x01000000
#define MUSL_IN_DONT_FOLLOW 0x02000000
#define MUSL_IN_EXCL_UNLINK 0x04000000
#define MUSL_IN_MASK_CREATE 0x10000000
#define MUSL_IN_MASK_ADD 0x20000000

#define MUSL_IN_ISDIR 0x40000000
#define MUSL_IN_ONESHOT 0x80000000

SB_EXPORT int __abi_wrap_inotify_init1(int flags);
SB_EXPORT int __abi_wrap_inotify_add_watch(int fd,
                                           const char* pathname,
                                           uint32_t mask);
SB_EXPORT int __abi_wrap_inotify_rm_watch(int fd, int wd);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_INOTIFY_ABI_WRAPPERS_H_
