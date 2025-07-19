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

#include "starboard/shared/modular/starboard_layer_posix_fcntl_abi_wrappers.h"

#include <fcntl.h>
#include <stdarg.h>

#include "starboard/common/log.h"

#include "starboard/shared/modular/starboard_layer_posix_fcntl_abi_wrappers.h"

#include <fcntl.h>
#include <stdarg.h>

#include "starboard/common/log.h"

#include "starboard/shared/modular/starboard_layer_posix_fcntl_abi_wrappers.h"

#include <fcntl.h>
#include <stdarg.h>

#include "starboard/common/log.h"

int musl_cmd_to_platform_cmd(int musl_cmd) {
  switch (musl_cmd) {
    case MUSL_F_DUPFD:
      return F_DUPFD;
    case MUSL_F_GETFD:
      return F_GETFD;
    case MUSL_F_SETFD:
      return F_SETFD;
    case MUSL_F_GETFL:
      return F_GETFL;
    case MUSL_F_SETFL:
      return F_SETFL;
    case MUSL_F_GETOWN:
      return F_GETOWN;
    case MUSL_F_SETOWN:
      return F_SETOWN;
    case MUSL_F_GETLK:
      return F_GETLK;
    case MUSL_F_SETLK:
      return F_SETLK;
    case MUSL_F_SETLKW:
      return F_SETLKW;
    // case MUSL_FD_CLOEXEC:
    //   return FD_CLOEXEC;
    // case MUSL_F_RDLCK:
    //   return F_RDLCK;
    // case MUSL_F_UNLCK:
    //   return F_UNLCK;
    // case MUSL_F_WRLCK:
    //   return F_UNLCK;
    default:
      SB_LOG(WARNING) << "Unknown musl fcntl command: " << musl_cmd;
      return -1;
  }
}

int musl_flag_to_platform_flag(int musl_flag) {
  // Special case: the MUSL_O_SYNC and MUSL_O_RSYNC share a value, so they can't
  // both be handled in the switch.
  if (musl_flag == MUSL_O_RSYNC) {
    return O_RSYNC;
  }

  switch (musl_flag) {
    case MUSL_O_APPEND:
      return O_APPEND;
    case MUSL_O_DSYNC:
      return O_DSYNC;
    case MUSL_O_NONBLOCK:
      return O_NONBLOCK;
    case MUSL_O_SYNC:
      return O_SYNC;
    case MUSL_O_PATH:
      return O_PATH;
    case MUSL_O_ACCMODE:
      return O_ACCMODE;
    case MUSL_O_RDONLY:
      return O_RDONLY;
    case MUSL_O_WRONLY:
      return O_WRONLY;
    case MUSL_O_RDWR:
      return O_RDWR;
    default:
      SB_LOG(WARNING) << "Unknown musl fcntl flag: " << musl_flag;
      return -1;
  }
}

SB_EXPORT int __abi_wrap_fcntl(int fd, int cmd, va_list args) {
  int platform_cmd = musl_cmd_to_platform_cmd(cmd);
  if (platform_cmd < 0) {
    return -1;
  }
  // The following commands have an int third argument.
  if (platform_cmd == F_DUPFD || platform_cmd == F_SETFD ||
      platform_cmd == F_SETFL || platform_cmd == F_SETOWN) {
    int val = va_arg(args, int);
    return fcntl(fd, platform_cmd, val);
  }

  // The following commands have a pointer third argument.
  if (platform_cmd == F_GETLK || platform_cmd == F_SETLK ||
      platform_cmd == F_SETLKW) {
    void* ptr = va_arg(args, void*);
    return fcntl(fd, platform_cmd, ptr);
  }

  // All other commands have no third argument.
  return fcntl(fd, platform_cmd);
}
