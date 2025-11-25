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

#include "starboard/shared/modular/starboard_layer_posix_uio_abi_wrappers.h"

#include <sys/uio.h>

ssize_t __abi_wrap_readv(int fildes, const struct musl_iovec* iov, int iovcnt) {
  struct iovec platform_iov[iovcnt];
  for (int i = 0; i < iovcnt; ++i) {
    platform_iov[i].iov_base = iov[i].iov_base;
    platform_iov[i].iov_len = iov[i].iov_len;
  }
  return readv(fildes, platform_iov, iovcnt);
}

ssize_t __abi_wrap_writev(int fildes,
                          const struct musl_iovec* iov,
                          int iovcnt) {
  struct iovec platform_iov[iovcnt];
  for (int i = 0; i < iovcnt; ++i) {
    platform_iov[i].iov_base = iov[i].iov_base;
    platform_iov[i].iov_len = iov[i].iov_len;
  }
  return writev(fildes, platform_iov, iovcnt);
}
