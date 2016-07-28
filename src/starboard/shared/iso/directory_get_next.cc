// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <dirent.h>
#include <errno.h>

#include "starboard/file.h"
#include "starboard/shared/iso/directory_internal.h"
#include "starboard/string.h"

bool SbDirectoryGetNext(SbDirectory directory, SbDirectoryEntry* out_entry) {
  if (!directory || !directory->directory || !out_entry) {
    return false;
  }

  struct dirent dirent_buffer;
  struct dirent* dirent;
  int result = readdir_r(directory->directory, &dirent_buffer, &dirent);
  if (result || !dirent) {
    return false;
  }

  SbStringCopy(out_entry->name, dirent->d_name,
               SB_ARRAY_SIZE_INT(out_entry->name));
  return true;
}
