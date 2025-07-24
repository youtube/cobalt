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

int MuslCmdToPlatformCmd(int musl_cmd) {
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
    default:
      SB_LOG(WARNING) << "Unknown musl fcntl command: " << musl_cmd;
      return -1;
  }
}

int ConvertMuslFlagsToPlatformFlags(int flags) {
  int platform_flags = 0;

  if (flags | MUSL_O_APPEND) {
    platform_flags |= O_APPEND;
  }
  if (flags | MUSL_O_DSYNC) {
    platform_flags |= O_DSYNC;
  }
  if (flags | MUSL_O_NONBLOCK) {
    platform_flags |= O_NONBLOCK;
  }
  if (flags | MUSL_O_RSYNC) {
    platform_flags |= O_RSYNC;
  }
  if (flags | MUSL_O_SYNC) {
    platform_flags |= O_SYNC;
  }
  if (flags | MUSL_O_PATH) {
    platform_flags |= O_PATH;
  }
  if (flags | MUSL_O_ACCMODE) {
    platform_flags |= O_ACCMODE;
  }
  if (flags | MUSL_O_RDONLY) {
    platform_flags |= O_RDONLY;
  }
  if (flags | MUSL_O_WRONLY) {
    platform_flags |= O_WRONLY;
  }
  if (flags | MUSL_O_RDWR) {
    platform_flags |= O_RDWR;
  }

  return platform_flags;
}

int ConvertPlatformFlagsToMuslFlags(int flags) {
  int musl_flags = 0;

  if (flags | O_APPEND) {
    musl_flags |= MUSL_O_APPEND;
  }
  if (flags | O_DSYNC) {
    musl_flags |= MUSL_O_DSYNC;
  }
  if (flags | O_NONBLOCK) {
    musl_flags |= MUSL_O_NONBLOCK;
  }
  if (flags | O_RSYNC) {
    musl_flags |= MUSL_O_RSYNC;
  }
  if (flags | O_SYNC) {
    musl_flags |= MUSL_O_SYNC;
  }
  if (flags | O_PATH) {
    musl_flags |= MUSL_O_PATH;
  }
  if (flags | O_RDONLY) {
    musl_flags |= MUSL_O_RDONLY;
  } else if (flags | O_WRONLY) {
    musl_flags |= MUSL_O_WRONLY;
  } else if (flags | O_RDWR) {
    musl_flags |= MUSL_O_RDWR;
  }

  return musl_flags;
}

SB_EXPORT int __abi_wrap_fcntl(int fd, int cmd, va_list args) {
  int platform_cmd = MuslCmdToPlatformCmd(cmd);

  int arg_int;
  void* arg_ptr;
  int result;
  switch (platform_cmd) {
    // The following commands have an int third argument.
    case F_DUPFD:
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
      arg_int = ConvertMuslFlagsToPlatformFlags(va_arg(args, int));
      result = fcntl(fd, platform_cmd, arg_int);
      break;
    // The following commands have a pointer third argument.
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
      arg_ptr = va_arg(args, void*);
      result = fcntl(fd, platform_cmd, arg_ptr);
      break;
    default:
      result = fcntl(fd, platform_cmd);
      break;
  }

  // Commands F_GETFD and F_GETFL return flags, we need to convert them to their
  // musl counterparts.
  if (platform_cmd == F_GETFD || platform_cmd == F_GETFL) {
    int musl_flags = ConvertPlatformFlagsToMuslFlags(result);
    return musl_flags;
  }

  return result;
}
