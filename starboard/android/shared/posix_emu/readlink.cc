// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <limits.h>
#include <unistd.h>

extern "C" {

ssize_t __real_readlink(const char* path, char* buf, size_t bufsize);

ssize_t __wrap_readlink(const char* path, char* buf, size_t bufsize) {
  // Passing a bufsize of 0 or one that wraps around size_t triggers a FORTIFY
  // arithmetic-overflow abort in bionic. Reject those up front with EINVAL.
  if (bufsize == 0 || bufsize > SSIZE_MAX) {
    errno = EINVAL;
    return -1;
  }
  return __real_readlink(path, buf, bufsize);
}

}  // extern "C"
