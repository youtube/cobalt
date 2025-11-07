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

#include <sys/uio.h>

extern "C" {

ssize_t __abi_wrap_readv(int fildes, const struct iovec* iov, int iovcnt);

ssize_t readv(int fildes, const struct iovec* iov, int iovcnt) {
  return __abi_wrap_readv(fildes, iov, iovcnt);
}

ssize_t __abi_wrap_writev(int fildes, const struct iovec* iov, int iovcnt);

ssize_t writev(int fildes, const struct iovec* iov, int iovcnt) {
  return __abi_wrap_writev(fildes, iov, iovcnt);
}
}
