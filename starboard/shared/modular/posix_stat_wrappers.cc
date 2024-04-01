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

#include "starboard/shared/modular/posix_stat_wrappers.h"

int __wrap_stat(const char* path, struct musl_stat* musl_info) {
  struct stat stat_info;  // The type from platform toolchain.
  int retval = stat(path, &stat_info);

  musl_info->st_mode = stat_info.st_mode;
  musl_info->st_size = stat_info.st_size;
  musl_info->st_atim.tv_sec = stat_info.st_atimespec.tv_sec;
  musl_info->st_atim.tv_nsec = stat_info.st_atimespec.tv_nsec;
  musl_info->st_mtim.tv_sec = stat_info.st_mtimespec.tv_sec;
  musl_info->st_mtim.tv_nsec = stat_info.st_mtimespec.tv_nsec;
  musl_info->st_ctim.tv_sec = stat_info.st_ctimespec.tv_sec;
  musl_info->st_ctim.tv_nsec = stat_info.st_ctimespec.tv_nsec;

  return retval;
}

