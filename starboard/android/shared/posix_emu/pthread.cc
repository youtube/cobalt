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
#include <sys/prctl.h>
#include <unistd.h>

extern "C" {

#if __ANDROID_API__ < 26
// The API doesn exist before API Level 26 and we currently target 24
// If this is the current thread we can obtain the name using `prctl`.
int pthread_getname_np(pthread_t thread, char* name, size_t len) {
  // The PR_GET_NAME expects a buffer of size 16 bytes.
  if (pthread_equal(pthread_self(), thread) && len >= 16) {
    prctl(PR_GET_NAME, name, 0L, 0L, 0L);
    return 0;
  }
  return -1;
}

#endif  // __ANDROID_API__ < 26
}
