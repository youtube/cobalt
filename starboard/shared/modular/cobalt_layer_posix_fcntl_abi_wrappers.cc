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

#include <fcntl.h>
#include <stdarg.h>

extern "C" {

int __abi_wrap_fcntl(int fildes, int cmd, ...);

int fcntl(int fildes, int cmd, ...) {
  int result;
  va_list ap;
  va_start(ap, cmd);
  switch (cmd) {
    // The following commands have an int third argument.
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
    case F_SETOWN_EX: {
      result = __abi_wrap_fcntl(fildes, cmd, va_arg(ap, int));
      break;
    }
    // The following commands have a pointer third argument.
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
    case F_OFD_GETLK:
    case F_OFD_SETLK:
    case F_OFD_SETLKW: {
      result = __abi_wrap_fcntl(fildes, cmd, va_arg(ap, void*));
    } break;
    default:
      result = __abi_wrap_fcntl(fildes, cmd);
      break;
  }

  va_end(ap);
  return result;
}

int __abi_wrap_openat(int fildes, const char* path, int oflag, ...);

int openat(int fildes, const char* path, int oflag, ...) {
  if ((oflag & O_CREAT) || (oflag & O_TMPFILE) == O_TMPFILE) {
    int result;
    va_list ap;
    va_start(ap, oflag);
    result = __abi_wrap_openat(fildes, path, oflag, va_arg(ap, mode_t));
    va_end(ap);
    return result;
  } else {
    return __abi_wrap_openat(fildes, path, oflag);
  }
}

}  // extern "C"
