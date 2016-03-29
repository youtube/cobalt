// Copyright 2016 Google Inc. All Rights Reserved.
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

// An implementation of storage that just uses the filesystem. The User
// implementation must implement a way to get the user's home directory.

#ifndef STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_

#include "starboard/file.h"
#include "starboard/shared/internal_only.h"
#include "starboard/storage.h"
#include "starboard/string.h"
#include "starboard/user.h"

struct SbStorageRecordPrivate {
  SbUser user;
  SbFile file;
};

namespace starboard {
namespace shared {
namespace starboard {
// Gets the path to the storage file for the given user.
SB_C_INLINE bool GetUserStorageFilePath(SbUser user,
                                        char* out_path,
                                        int path_size) {
  bool success = SbUserGetProperty(user, kSbUserPropertyHomeDirectory, out_path,
                                   path_size);
  if (!success) {
    return false;
  }

  SbStringConcat(out_path, "/.starboard.storage", path_size);
  return true;
}
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
