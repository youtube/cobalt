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

// clang-format off
#include <pthread.h>
// clang-format on

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "starboard/shared/posix/handle_eintr.h"

extern "C" {

#if __ANDROID_API__ < 26
int pthread_getname_np(pthread_t thread, char* name, size_t len) {
  // The kernel's limit on thread names, terminator included.
  constexpr size_t kMaxThreadNameLen = 16;

  if (len < kMaxThreadNameLen) {
    return ERANGE;
  }

  if (pthread_equal(thread, pthread_self())) {
    return prctl(PR_GET_NAME, name, 0L, 0L, 0L) == -1 ? errno : 0;
  }

  // Where the kernel exposes another thread's name. A tid has at most 10
  // digits, 8 more than the "%d" placeholder.
  constexpr char kThreadNamePath[] = "/proc/self/task/%d/comm";
  constexpr size_t kThreadNamePathLen = sizeof(kThreadNamePath) + 8;

  char path[kThreadNamePathLen];
  snprintf(path, sizeof(path), kThreadNamePath, pthread_gettid_np(thread));
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    return errno;
  }

  ssize_t count = HANDLE_EINTR(read(fd, name, len));
  int read_errno = errno;
  close(fd);
  if (count == -1) {
    return read_errno;
  }

  // The kernel terminates the name with a newline.
  if (count > 0 && name[count - 1] == '\n') {
    name[count - 1] = '\0';
    return 0;
  }

  if (static_cast<size_t>(count) == len) {
    return ERANGE;
  }

  name[count] = '\0';
  return 0;
}
#endif  // __ANDROID_API__ < 26

}  // extern "C"
