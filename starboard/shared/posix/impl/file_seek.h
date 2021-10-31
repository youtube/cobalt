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

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_SEEK_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_SEEK_H_

#include "starboard/file.h"

#include <unistd.h>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/posix/impl/file_impl.h"

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

int64_t FileSeek(SbFile file, SbFileWhence whence, int64_t offset) {
  if (!file || file->descriptor < 0) {
    return -1;
  }

  return lseek(file->descriptor, static_cast<off_t>(offset),
               static_cast<int>(whence));
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_SEEK_H_
