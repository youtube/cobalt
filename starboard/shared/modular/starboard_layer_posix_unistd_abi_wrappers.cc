// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_unistd_abi_wrappers.h"

#include <sys/types.h>
#include <unistd.h>

int __abi_wrap_ftruncate(int fildes, musl_off_t length) {
  return ftruncate(fildes, static_cast<off_t>(length));
}

musl_off_t __abi_wrap_lseek(int fildes, musl_off_t offset, int whence) {
  return static_cast<off_t>(lseek(fildes, static_cast<off_t>(offset), whence));
}

ssize_t __abi_wrap_read(int fildes, void* buf, size_t nbyte) {
  return read(fildes, buf, nbyte);
}

ssize_t __abi_wrap_write(int fildes, const void* buf, size_t nbyte) {
  return write(fildes, buf, nbyte);
}
