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

// An implementation of storage that just uses the filesystem. The User
// implementation must implement a way to get the user's home directory.

#ifndef STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_

#include "starboard/common/storage.h"
#include "starboard/common/string.h"
#include "starboard/file.h"
#include "starboard/shared/internal_only.h"
#include "starboard/user.h"

#if SB_HAS_QUIRK(HASH_FILE_NAME)
#include <ios>
#include <sstream>
#include <string>
#include "starboard/common/murmurhash2.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/memory.h"
#endif

struct SbStorageRecordPrivate {
  SbUser user;
  SbFile file;
  std::string name;
};

namespace starboard {
namespace shared {
namespace starboard {
// Gets the path to the storage file for the given user.
static SB_C_INLINE bool GetUserStorageFilePath(SbUser user,
                                               const char* name,
                                               char* out_path,
                                               int path_size) {
  bool success = SbUserGetProperty(user, kSbUserPropertyHomeDirectory, out_path,
                                   path_size);
  if (!success) {
    return false;
  }

  const size_t n = name ? strlen(name) : 0;
  ::starboard::strlcat(out_path, "/.starboard", path_size);
  if (n > 0) {
    ::starboard::strlcat(out_path, ".", path_size);
#if SB_HAS_QUIRK(HASH_FILE_NAME)
    size_t n = strlen(name);
    // Two 32 bit hashes will create a 64 bit hash with extremely low
    // probability of collisions. The seed term was chosen arbitrary.
    uint32_t hash1 = MurmurHash2_32(name, n, 0x8df88a67);
    uint32_t hash2 = MurmurHash2_32(name, n, 0x5bdac960);
    std::stringstream name_stringstream;
    name_stringstream << std::hex << hash1 << hash2;
    ::starboard::strlcat(out_path, name_stringstream.str().c_str(), path_size);
#else
    ::starboard::strlcat(out_path, name, path_size);
#endif
  }
  ::starboard::strlcat(out_path, ".storage", path_size);
  return true;
}
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_FILE_STORAGE_STORAGE_INTERNAL_H_
