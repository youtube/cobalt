// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/directory.h"

#include <android/asset_manager.h>

#include "starboard/android/shared/directory_internal.h"
#include "starboard/shared/iso/impl/directory_get_next.h"

bool SbDirectoryGetNext(SbDirectory directory, SbDirectoryEntry* out_entry) {
  if (directory && directory->asset_dir && out_entry) {
    const char* file_name = AAssetDir_getNextFileName(directory->asset_dir);
    if (file_name == NULL) {
      return false;
    }
    size_t size = SB_ARRAY_SIZE_INT(out_entry->name);
    SbStringCopy(out_entry->name, file_name, size);
    return true;
  }

  return ::starboard::shared::iso::impl::SbDirectoryGetNext(directory,
                                                            out_entry);
}
