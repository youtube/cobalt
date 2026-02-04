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

#include <sys/stat.h>

extern "C" {

int __abi_wrap_fstat(int fildes, struct stat* info);

int fstat(int fildes, struct stat* info) {
  return __abi_wrap_fstat(fildes, info);
}

int __abi_wrap_chmod(const char* path, mode_t mode);

int chmod(const char* path, mode_t mode) {
  return __abi_wrap_chmod(path, mode);
}

int __abi_wrap_fchmod(int fd, mode_t mode);

int fchmod(int fd, mode_t mode) {
  return __abi_wrap_fchmod(fd, mode);
}

int __abi_wrap_utimensat(int fildes,
                         const char* path,
                         const struct timespec times[2],
                         int flag);

int utimensat(int fildes,
              const char* path,
              const struct timespec times[2],
              int flag) {
  return __abi_wrap_utimensat(fildes, path, times, flag);
}

int __abi_wrap_fstatat(int fildes,
                       const char* path,
                       struct stat* info,
                       int flag);

int fstatat(int fildes, const char* path, struct stat* info, int flag) {
  return __abi_wrap_fstatat(fildes, path, info, flag);
}
}
