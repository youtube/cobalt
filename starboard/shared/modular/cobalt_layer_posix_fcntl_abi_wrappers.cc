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

int __abi_wrap_fcntl(int fildes, int cmd, va_list args);

int fcntl(int fildesss, int cmd, ...) {
  va_list args;
  va_start(args, cmd);
  int result = __abi_wrap_fcntl(fildes, cmd, args);
  va_end(args);
  return result;
}

}  // extern "C"
