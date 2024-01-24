// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// Adapted from base/platform_file_posix.cc

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_RENAME_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_RENAME_H_

#include "starboard/file.h"

#include <stdio.h>
#include <sys/stat.h>

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

#if defined(OS_BSD) || defined(OS_MACOSX) || defined(OS_NACL) || \
    defined(OS_FUCHSIA) || (defined(OS_ANDROID) && __ANDROID_API__ < 21)
typedef struct stat stat_wrapper_t;

int CallStat(const char* path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return stat(path, sb);
}
#else
typedef struct stat64 stat_wrapper_t;

int CallStat(const char* path, stat_wrapper_t* sb) {
  return stat64(path, sb);
}
#endif

bool FileRename(const char* from_path, const char* to_path) {
  stat_wrapper_t to_file_info;
  // Windows compatibility: if |to_path| exists, |from_path| and |to_path|
  // must be the same type, either both files, or both directories.
  if (CallStat(to_path, &to_file_info) == 0) {
    stat_wrapper_t from_file_info;
    if (CallStat(from_path, &from_file_info) != 0)
      return false;
    if (S_ISDIR(to_file_info.st_mode) != S_ISDIR(from_file_info.st_mode))
      return false;
  }

  if (rename(from_path, to_path) == 0)
    return true;

  return false;
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_RENAME_H_
