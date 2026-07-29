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

#include <stdint.h>
#include <sys/inotify.h>

extern "C" {

int __abi_wrap_inotify_init1(int flags);
int __abi_wrap_inotify_add_watch(int fd, const char* pathname, uint32_t mask);
int __abi_wrap_inotify_rm_watch(int fd, int wd);

int inotify_init1(int flags) {
  return __abi_wrap_inotify_init1(flags);
}

int inotify_add_watch(int fd, const char* pathname, uint32_t mask) {
  return __abi_wrap_inotify_add_watch(fd, pathname, mask);
}

int inotify_rm_watch(int fd, int wd) {
  return __abi_wrap_inotify_rm_watch(fd, wd);
}

}  // extern "C"
