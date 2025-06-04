// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// An implementation of storage that just uses the filesystem.

#ifndef STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_

#include <string>

#include "starboard/common/storage.h"
#include "starboard/common/string.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/get_home_directory.h"

struct SbStorageRecordPrivate {
  void* unused_user;  // deprecated in SB 16
  int file;
  std::string name;
};

namespace starboard::shared::starboard {
// Gets the path to the storage file.
static inline bool GetStorageFilePath(const char* name,
                                      char* out_path,
                                      int path_size) {
  bool success = GetHomeDirectory(out_path, path_size);
  if (!success) {
    return false;
  }

  const size_t n = name ? strlen(name) : 0;
  ::starboard::strlcat(out_path, "/.starboard", path_size);
  if (n > 0) {
    ::starboard::strlcat(out_path, ".", path_size);
    ::starboard::strlcat(out_path, name, path_size);
  }
  ::starboard::strlcat(out_path, ".storage", path_size);
  return true;
}

}  // namespace starboard::shared::starboard

#endif  // STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
