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

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_CAN_OPEN_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_CAN_OPEN_H_

#include "starboard/file.h"

#include <errno.h>
#include <sys/stat.h>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/posix/impl/file_impl.h"

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

bool FileCanOpen(const char* path, int flags) {
  bool can_read = flags & kSbFileRead;
  bool can_write = flags & kSbFileWrite;
  if (!can_read && !can_write) {
    return false;
  }

  struct stat file_info;
  if (stat(path, &file_info) != 0) {
    return false;
  }

  if (can_read && !(file_info.st_mode & S_IRUSR)) {
    errno = EACCES;
    return false;
  }

  if (can_write && !(file_info.st_mode & S_IWUSR)) {
    errno = EACCES;
    return false;
  }

  return true;
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_CAN_OPEN_H_
