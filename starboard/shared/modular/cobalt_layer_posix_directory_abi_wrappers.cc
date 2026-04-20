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

#include <dirent.h>

extern "C" {

int __abi_wrap_readdir_r(DIR* dirp,
                         struct dirent* entry,
                         struct dirent** result);

int readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result) {
  return __abi_wrap_readdir_r(dirp, entry, result);
}

DIR* __abi_wrap_opendir(const char* name);

DIR* opendir(const char* name) {
  return __abi_wrap_opendir(name);
}

int __abi_wrap_closedir(DIR* directory);

int closedir(DIR* directory) {
  return __abi_wrap_closedir(directory);
}

struct dirent* __abi_wrap_readdir(DIR* dirp);

struct dirent* readdir(DIR* dirp) {
  return __abi_wrap_readdir(dirp);
}

DIR* __abi_wrap_fdopendir(int fd);
DIR* fdopendir(int fd) {
  return __abi_wrap_fdopendir(fd);
}
}
