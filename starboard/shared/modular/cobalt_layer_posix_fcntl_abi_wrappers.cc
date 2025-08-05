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

int Fcntl(int fildes, int cmd);
int FcntlInt(int fildes, int cmd, int arg);
int FcntlPtr(int fildes, int cmd, void* arg);

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
    case F_SETOWN: {
      int arg_int = va_arg(ap, int);
      result = FcntlInt(fildes, cmd, arg_int);
      break;
    }
    // The following commands have a pointer third argument.
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW: {
      void* arg_ptr;
      arg_ptr = va_arg(ap, void*);
      result = FcntlPtr(fildes, cmd, arg_ptr);
    } break;
    default:
      result = Fcntl(fildes, cmd);
      break;
  }
  va_end(ap);
  return result;
}

}  // extern "C"
