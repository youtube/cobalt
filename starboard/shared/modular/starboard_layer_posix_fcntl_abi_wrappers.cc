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
  SB_LOG(INFO) << "Converting musl cmd " << musl_cmd;
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

  if (flags & MUSL_O_APPEND) {
    platform_flags |= O_APPEND;
  }
  if (flags & MUSL_O_DSYNC) {
    platform_flags |= O_DSYNC;
  }
  if (flags & MUSL_O_NONBLOCK) {
    platform_flags |= O_NONBLOCK;
  }
  if (flags & MUSL_O_RSYNC) {
    platform_flags |= O_RSYNC;
  }
  if (flags & MUSL_O_SYNC) {
    platform_flags |= O_SYNC;
  }
  if (flags & MUSL_O_ACCMODE) {
    platform_flags |= O_ACCMODE;
  }
  if (flags & MUSL_O_RDONLY) {
    platform_flags |= O_RDONLY;
  }
  if (flags & MUSL_O_WRONLY) {
    platform_flags |= O_WRONLY;
  }
  if (flags & MUSL_O_RDWR) {
    platform_flags |= O_RDWR;
  }
  if (flags & MUSL_FD_CLOEXEC) {
    platform_flags |= FD_CLOEXEC;
  }
  if (flags & MUSL_F_RDLCK) {
    platform_flags |= F_RDLCK;
  }
  if (flags & MUSL_F_WRLCK) {
    platform_flags |= F_WRLCK;
  }
  if (flags & MUSL_F_UNLCK) {
    platform_flags |= F_UNLCK;
  }

  return platform_flags;
}

int ConvertPlatformFlagsToMuslFlags(int flags) {
  int musl_flags = 0;

  if (flags & O_APPEND) {
    musl_flags |= MUSL_O_APPEND;
  }
  if (flags & O_DSYNC) {
    musl_flags |= MUSL_O_DSYNC;
  }
  if (flags & O_NONBLOCK) {
    musl_flags |= MUSL_O_NONBLOCK;
  }
  if (flags & O_RSYNC) {
    musl_flags |= MUSL_O_RSYNC;
  }
  if (flags & O_SYNC) {
    musl_flags |= MUSL_O_SYNC;
  }
  if (flags & O_RDONLY) {
    musl_flags |= MUSL_O_RDONLY;
  } else if (flags & O_WRONLY) {
    musl_flags |= MUSL_O_WRONLY;
  } else if (flags & O_RDWR) {
    musl_flags |= MUSL_O_RDWR;
  }
  if (flags & FD_CLOEXEC) {
    musl_flags |= MUSL_FD_CLOEXEC;
  }
  if (flags & MUSL_F_RDLCK) {
    musl_flags |= MUSL_F_RDLCK;
  }
  if (flags & MUSL_F_WRLCK) {
    musl_flags |= MUSL_F_WRLCK;
  }
  if (flags & MUSL_F_UNLCK) {
    musl_flags |= MUSL_F_UNLCK;
  }

  return musl_flags;
}

SB_EXPORT int __abi_wrap_fcntl(int fd, int cmd, ...) {
  int platform_cmd = MuslCmdToPlatformCmd(cmd);
  SB_LOG(INFO) << "musl cmd " << cmd << " platform cmd " << platform_cmd;
  int arg_int;
  void* arg_ptr;
  int result;
  va_list ap;
  va_start(ap, cmd);
  switch (platform_cmd) {
    // The following commands have an int third argument.
    case F_DUPFD:
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
      arg_int = ConvertMuslFlagsToPlatformFlags(va_arg(ap, int));
      result = fcntl(fd, platform_cmd, arg_int);
      break;
    // The following commands have a pointer third argument.
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW: {
      arg_ptr = va_arg(ap, void*);
      SB_CHECK(arg_ptr);
      struct musl_flock* musl_flock =
          reinterpret_cast<struct musl_flock*>(arg_ptr);
      struct flock platform_flock;
      if (platform_cmd == F_GETLK) {
        SB_LOG(INFO) << "GETLK";
        platform_flock.l_type =
            ConvertMuslFlagsToPlatformFlags(musl_flock->l_type);
        SB_LOG(INFO) << "MUSL L_TYPE: " << musl_flock->l_type
                     << " platform l_type: " << platform_flock.l_type;
        platform_flock.l_whence = musl_flock->l_whence;
        platform_flock.l_start = musl_flock->l_start;
        platform_flock.l_len = musl_flock->l_len;
        platform_flock.l_pid = musl_flock->l_pid;
        result = fcntl(fd, platform_cmd, &platform_flock);
        SB_LOG(INFO) << "result " << result;
        musl_flock->l_type =
            ConvertPlatformFlagsToMuslFlags(platform_flock.l_type);
        musl_flock->l_whence = platform_flock.l_whence;
        musl_flock->l_start = platform_flock.l_start;
        musl_flock->l_len = platform_flock.l_len;
        musl_flock->l_pid = platform_flock.l_pid;
      } else {
        platform_flock.l_type = musl_flock->l_type;
        platform_flock.l_whence = musl_flock->l_whence;
        platform_flock.l_start = musl_flock->l_start;
        platform_flock.l_len = musl_flock->l_len;
        platform_flock.l_pid = musl_flock->l_pid;
        result = fcntl(fd, platform_cmd, &platform_flock);
      }
      break;
    }
    default:
      result = fcntl(fd, platform_cmd);
      break;
  }
  va_end(ap);

  // Commands F_GETFD and F_GETFL return flags, we need to convert them to their
  // musl counterparts.
  if (platform_cmd == F_GETFD || platform_cmd == F_GETFL) {
    int musl_flags = ConvertPlatformFlagsToMuslFlags(result);
    return musl_flags;
  }

  return result;
}

int __abi_wrap_fcntl2(int fildes, int cmd) {
  SB_LOG(INFO) << "In __abi_wrap_fcntl2";
  int platform_cmd = MuslCmdToPlatformCmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  int result = fcntl(fildes, platform_cmd);
  if (platform_cmd == F_GETFD || platform_cmd == F_GETFL) {
    SB_LOG(INFO) << "Converting flags back to musl";
    int musl_flags = ConvertPlatformFlagsToMuslFlags(result);

    return musl_flags;
  }

  return result;
}

int __abi_wrap_fcntl3(int fildes, int cmd, int arg) {
  SB_LOG(INFO) << "In __abi_wrap_fcntl3";
  int platform_cmd = MuslCmdToPlatformCmd(cmd);
  SB_LOG(INFO) << "Called fcntl2 with cmd " << cmd << "platform cmd "
               << platform_cmd << " fildes " << fildes << " and arg " << arg;
  if (platform_cmd == -1) {
    errno = EINVAL;
    SB_LOG(INFO) << "EINVAL";
    return -1;
  }

  return fcntl(fildes, platform_cmd, arg);
}

int __abi_wrap_fcntl4(int fildes, int cmd, void* arg) {
  SB_LOG(INFO) << "In __abi_wrap_fcntl4";
  SB_CHECK(arg);
  int platform_cmd = MuslCmdToPlatformCmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  struct musl_flock* musl_flock = reinterpret_cast<struct musl_flock*>(arg);
  struct flock platform_flock;

  SB_CHECK(platform_cmd == F_GETLK || platform_cmd == F_SETLK ||
           platform_cmd == F_SETLKW);
  int retval;
  if (platform_cmd == MUSL_F_GETLK) {
    retval = fcntl(fildes, platform_cmd, &platform_flock);
    musl_flock->l_type = platform_flock.l_type;
    musl_flock->l_whence = platform_flock.l_whence;
    musl_flock->l_start = platform_flock.l_start;
    musl_flock->l_len = platform_flock.l_len;
    musl_flock->l_pid = platform_flock.l_pid;
  } else {
    platform_flock.l_type = musl_flock->l_type;
    platform_flock.l_whence = musl_flock->l_whence;
    platform_flock.l_start = musl_flock->l_start;
    platform_flock.l_len = musl_flock->l_len;
    platform_flock.l_pid = musl_flock->l_pid;
    retval = fcntl(fildes, platform_cmd, &platform_flock);
  }

  return retval;
}
