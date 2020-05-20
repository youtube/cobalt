// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/file_atomic_replace_write_file.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/file.h"

#if SB_API_VERSION >= 12

namespace starboard {
namespace shared {
namespace starboard {

bool SbFileAtomicReplaceWriteFile(const char* path,
                                  const char* data,
                                  int64_t data_size) {
  SbFileError error;
  SbFile temp_file = SbFileOpen(
      path, kSbFileCreateAlways | kSbFileWrite | kSbFileRead, NULL, &error);

  if (error != kSbFileOk) {
    return false;
  }

  SbFileTruncate(temp_file, 0);

  const char* source = data;
  int64_t to_write = data_size;

  while (to_write > 0) {
    const int to_write_max =
        static_cast<int>(std::min(to_write, static_cast<int64_t>(kSbInt32Max)));
    const int bytes_written = SbFileWrite(temp_file, source, to_write_max);

    if (bytes_written < 0) {
      SbFileClose(temp_file);
      SbFileDelete(path);
      return false;
    }

    source += bytes_written;
    to_write -= bytes_written;
  }

  SbFileFlush(temp_file);

  if (!SbFileClose(temp_file)) {
    return false;
  }
  return true;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // SB_API_VERSION >= 12
