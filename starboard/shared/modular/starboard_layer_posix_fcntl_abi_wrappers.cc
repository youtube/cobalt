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
#include "starboard/shared/modular/starboard_layer_posix_stat_abi_wrappers.h"

int convert_musl_cmd_to_platform_cmd(int musl_cmd) {
  switch (musl_cmd) {
    case MUSL_F_DUPFD:
      return F_DUPFD;
    case MUSL_F_DUPFD_CLOEXEC:
      return F_DUPFD_CLOEXEC;
    case MUSL_F_GETFD:
      return F_GETFD;
    case MUSL_F_SETFD:
      return F_SETFD;
    case MUSL_F_GETFL:
      return F_GETFL;
    case MUSL_F_SETFL:
      return F_SETFL;
    case MUSL_F_GETLK:
      return F_GETLK;
    case MUSL_F_SETLK:
      return F_SETLK;
    case MUSL_F_SETLKW:
      return F_SETLKW;
    case MUSL_F_GETOWN:
      return F_GETOWN;
    case MUSL_F_SETOWN:
      return F_SETOWN;
    case MUSL_F_GETOWN_EX:
      return F_GETOWN_EX;
    case MUSL_F_SETOWN_EX:
      return F_SETOWN_EX;
    case MUSL_F_OFD_GETLK:
      return F_OFD_GETLK;
    case MUSL_F_OFD_SETLK:
      return F_OFD_SETLK;
    case MUSL_F_OFD_SETLKW:
      return F_OFD_SETLKW;
    default:
      SB_LOG(WARNING) << "Unknown musl fcntl command: " << musl_cmd;
      return -1;
  }
}

int convert_musl_flags_to_platform_flags(int flags) {
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
  if (flags & MUSL_O_PATH) {
    platform_flags |= O_PATH;
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
  if (flags & O_PATH) {
    musl_flags |= MUSL_O_PATH;
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

short convert_musl_flock_type_to_platform(short musl_type) {
  switch (musl_type) {
    case MUSL_F_RDLCK:
      return F_RDLCK;
    case MUSL_F_WRLCK:
      return F_WRLCK;
    case MUSL_F_UNLCK:
      return F_UNLCK;
    default:
      SB_LOG(WARNING) << "Unknown musl flock type: " << musl_type;
      return -1;
  }
}

short convert_platform_flock_type_to_musl(short platform_type) {
  switch (platform_type) {
    case F_RDLCK:
      return MUSL_F_RDLCK;
    case F_WRLCK:
      return MUSL_F_WRLCK;
    case F_UNLCK:
      return MUSL_F_UNLCK;
    default:
      SB_LOG(WARNING) << "Unknown platform flock type: " << platform_type;
      return -1;
  }
}

short convert_musl_flock_whence_to_platform(short musl_whence) {
  switch (musl_whence) {
    case MUSL_SEEK_SET:
      return SEEK_SET;
    case MUSL_SEEK_CUR:
      return SEEK_CUR;
    case MUSL_SEEK_END:
      return SEEK_END;
    default:
      SB_LOG(WARNING) << "Unknown musl flock whence: " << musl_whence;
      return -1;
  }
}

short convert_platform_flock_whence_to_musl(short platform_whence) {
  switch (platform_whence) {
    case SEEK_SET:
      return MUSL_SEEK_SET;
    case SEEK_CUR:
      return MUSL_SEEK_CUR;
    case SEEK_END:
      return MUSL_SEEK_END;
    default:
      SB_LOG(WARNING) << "Unknown platform flock whence: " << platform_whence;
      return -1;
  }
}

int fcntl_ptr(int fildes, int cmd, void* arg) {
  SB_CHECK(arg);
  SB_CHECK(cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW ||
           cmd == F_OFD_GETLK || cmd == F_OFD_SETLK || cmd == F_OFD_SETLKW);

  struct musl_flock* musl_flock_ptr = reinterpret_cast<struct musl_flock*>(arg);
  struct flock platform_flock;

  // Translate from musl_flock to platform_flock.
  platform_flock.l_type =
      convert_musl_flock_type_to_platform(musl_flock_ptr->l_type);
  platform_flock.l_whence =
      convert_musl_flock_whence_to_platform(musl_flock_ptr->l_whence);
  platform_flock.l_start = musl_flock_ptr->l_start;
  platform_flock.l_len = musl_flock_ptr->l_len;
  platform_flock.l_pid = musl_flock_ptr->l_pid;

  if (platform_flock.l_type == -1 || platform_flock.l_whence == -1) {
    errno = EINVAL;
    return -1;
  }

  int retval = fcntl(fildes, cmd, &platform_flock);

  // For GETLK commands, translate the result back.
  if ((cmd == F_GETLK || cmd == F_OFD_GETLK) && retval != -1) {
    musl_flock_ptr->l_type =
        convert_platform_flock_type_to_musl(platform_flock.l_type);
    musl_flock_ptr->l_whence =
        convert_platform_flock_whence_to_musl(platform_flock.l_whence);
    musl_flock_ptr->l_start = platform_flock.l_start;
    musl_flock_ptr->l_len = platform_flock.l_len;
    musl_flock_ptr->l_pid = platform_flock.l_pid;
  }

  return retval;
}

SB_EXPORT int __abi_wrap_fcntl(int fildes, int cmd, ...) {
  int platform_cmd = convert_musl_cmd_to_platform_cmd(cmd);
  if (platform_cmd == -1) {
    errno = EINVAL;
    return -1;
  }

  int result;
  va_list ap;
  va_start(ap, cmd);
  switch (platform_cmd) {
    // The following commands have an int third argument.
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
    case F_SETOWN_EX: {
      int arg = va_arg(ap, int);
      if (platform_cmd == F_SETFD || platform_cmd == F_SETFL) {
        result = fcntl(fildes, platform_cmd,
                       convert_musl_flags_to_platform_flags(arg));
      } else {
        result = fcntl(fildes, platform_cmd, arg);
      }
      break;
    }
    // The following commands have a pointer third argument.
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
    case F_OFD_GETLK:
    case F_OFD_SETLK:
    case F_OFD_SETLKW: {
      void* arg = va_arg(ap, void*);
      SB_CHECK(arg);
      result = fcntl_ptr(fildes, platform_cmd, arg);
    } break;
    default:
      SB_CHECK(platform_cmd == F_GETFD || platform_cmd == F_GETFL ||
               platform_cmd == F_GETOWN || platform_cmd == F_SETFL ||
               platform_cmd == F_SETOWN);
      result = fcntl(fildes, platform_cmd);
      if (platform_cmd == F_GETFD || platform_cmd == F_GETFL) {
        int musl_flags = convert_platform_flags_to_musl_flags(result);

        result = musl_flags;
      }
      break;
  }

  va_end(ap);
  return result;
}

int __abi_wrap_openat(int fildes, const char* path, int oflag, ...) {
  fildes = (fildes == MUSL_AT_FDCWD) ? AT_FDCWD : fildes;
  if ((oflag & O_CREAT) || (oflag & O_TMPFILE) == O_TMPFILE) {
    va_list ap;
    va_start(ap, oflag);
    musl_mode_t mode = va_arg(ap, musl_mode_t);
    return openat(fildes, path, oflag,
                  starboard::musl_mode_to_platform_mode(mode));
  } else {
    return openat(fildes, path, oflag);
  }
}
