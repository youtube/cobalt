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

int convert_musl_cmd_to_platform_cmd(int musl_cmd) {
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

int convert_musl_flags_to_platofrm_flags(int flags) {
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
  if (flags & MUSL_F_DUPFD_CLOEXEC) {
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

int convert_platform_flags_to_musl_flags(int flags) {
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
    musl_flags |= MUSL_F_DUPFD_CLOEXEC;
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

SB_EXPORT int fcntl_no_arg(int fildes, int cmd) {
  int platform_cmd = convert_musl_cmd_to_platform_cmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  int result = fcntl(fildes, platform_cmd);
  if (platform_cmd == F_GETFD || platform_cmd == F_GETFL) {
    int musl_flags = convert_platform_flags_to_musl_flags(result);

    return musl_flags;
  }

  return result;
}

SB_EXPORT int fcntl_int_arg(int fildes, int cmd, int arg) {
  int platform_cmd = convert_musl_cmd_to_platform_cmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  return fcntl(fildes, platform_cmd, arg);
}

SB_EXPORT int fcntl_ptr_arg(int fildes, int cmd, void* arg) {
  SB_CHECK(arg);
  SB_CHECK(cmd == MUSL_F_GETLK || cmd == MUSL_F_SETLK || cmd == MUSL_F_SETLKW);

  int platform_cmd = convert_musl_cmd_to_platform_cmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  struct musl_flock* musl_flock = reinterpret_cast<struct musl_flock*>(arg);
  struct flock platform_flock;
  int retval;
  if (platform_cmd == F_GETLK) {
    retval =
        fcntl(fildes, platform_cmd, reinterpret_cast<void*>(&platform_flock));
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

SB_EXPORT int __abi_wrap_fcntl(int fd, int cmd, ...) {
  int result;
  va_list ap;
  va_start(ap, cmd);
  switch (cmd) {
    // The following commands have an int third argument.
    case MUSL_F_DUPFD:
    case MUSL_F_DUPFD_CLOEXEC:
    case MUSL_F_SETFD:
    case MUSL_F_SETFL:
    case MUSL_F_SETOWN: {
      int arg_int = va_arg(ap, int);
      result = fcntl_int_arg(fd, cmd, arg_int);
      break;
    }
    // The following commands have a pointer third argument.
    case MUSL_F_GETLK:
    case MUSL_F_SETLK:
    case MUSL_F_SETLKW: {
      void* arg_ptr;
      arg_ptr = va_arg(ap, void*);
      SB_CHECK(arg_ptr);
      result = fcntl_ptr_arg(fd, cmd, arg_ptr);
    } break;
    default:
      result = fcntl_no_arg(fd, cmd);
      break;
  }
  va_end(ap);

  // Commands F_GETFD and F_GETFL return flags, we need to convert them to their
  // musl counterparts.
  if (cmd == MUSL_F_GETFD || cmd == MUSL_F_GETFL) {
    int musl_flags = convert_platform_flags_to_musl_flags(result);
    return musl_flags;
  }

  return result;
}
