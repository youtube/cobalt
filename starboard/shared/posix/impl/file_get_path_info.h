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

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_GET_PATH_INFO_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_GET_PATH_INFO_H_

#include "starboard/file.h"

#include <sys/stat.h>

#include "starboard/shared/posix/time_internal.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/posix/impl/file_impl.h"

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

bool FileGetPathInfo(const char* path, SbFileInfo* out_info) {
  if (!path || path[0] == '\0' || !out_info) {
    return false;
  }

  struct stat file_info;
  int result = stat(path, &file_info);
  if (result) {
    return false;
  }

  out_info->creation_time = FromTimeT(file_info.st_ctime);
  out_info->is_directory = S_ISDIR(file_info.st_mode);
  out_info->is_symbolic_link = S_ISLNK(file_info.st_mode);
  out_info->last_accessed = FromTimeT(file_info.st_atime);
  out_info->last_modified = FromTimeT(file_info.st_mtime);
  out_info->size = file_info.st_size;
  return true;
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_GET_PATH_INFO_H_
