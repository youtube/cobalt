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
#ifndef STARBOARD_SHARED_ISO_IMPL_DIRECTORY_GET_NEXT_H_
#define STARBOARD_SHARED_ISO_IMPL_DIRECTORY_GET_NEXT_H_
#if SB_API_VERSION < 17

#include "starboard/directory.h"

#include <dirent.h>
#include <errno.h>

#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/iso/impl/directory_impl.h"

namespace starboard {
namespace shared {
namespace iso {
namespace impl {

bool SbDirectoryGetNext(SbDirectory directory,
                        char* out_entry,
                        size_t out_entry_size) {
  if (out_entry_size < kSbFileMaxName) {
    return false;
  }

  if (!directory || !directory->directory || !out_entry) {
    return false;
  }

  struct dirent dirent_buffer;
  struct dirent* dirent;
  int result = readdir_r(directory->directory, &dirent_buffer, &dirent);
  if (result || !dirent) {
    return false;
  }

  starboard::strlcpy(out_entry, dirent->d_name, out_entry_size);
  return true;
}

}  // namespace impl
}  // namespace iso
}  // namespace shared
}  // namespace starboard
#endif  // SB_API_VERSION < 17

#endif  // STARBOARD_SHARED_ISO_IMPL_DIRECTORY_GET_NEXT_H_
