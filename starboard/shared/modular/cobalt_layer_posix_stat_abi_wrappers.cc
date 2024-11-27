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
int __abi_wrap_stat(const char* path, struct stat* info);

int fstat(int fildes, struct stat* info) {
  return __abi_wrap_fstat(fildes, info);
}

int stat(const char* path, struct stat* info) {
  return __abi_wrap_stat(path, info);
}
}
