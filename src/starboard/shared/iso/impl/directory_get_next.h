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

#include "starboard/directory.h"

#include <dirent.h>
#include <errno.h>

#include "starboard/common/string.h"
#include "starboard/file.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/iso/impl/directory_impl.h"

namespace starboard {
namespace shared {
namespace iso {
namespace impl {

bool SbDirectoryGetNext(SbDirectory directory, SbDirectoryEntry* out_entry) {
  if (!directory || !directory->directory || !out_entry) {
    return false;
  }

  // Look for the first directory that isn't current or parent.
  struct dirent dirent_buffer;
  struct dirent* dirent;
  int result;
  do {
    result = readdir_r(directory->directory, &dirent_buffer, &dirent);
    if (!result && dirent) {
      if ((SbStringCompareAll(dirent->d_name, ".") == 0) ||
          (SbStringCompareAll(dirent->d_name, "..") == 0)) {
        continue;
      } else {
        break;
      }
    } else {
      return false;
    }
  } while (true);

  SbStringCopy(out_entry->name, dirent->d_name,
               SB_ARRAY_SIZE_INT(out_entry->name));
  return true;
}

}  // namespace impl
}  // namespace iso
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_ISO_IMPL_DIRECTORY_GET_NEXT_H_
