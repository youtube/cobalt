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

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/file.h"

namespace starboard::shared::starboard {

bool SbFileAtomicReplaceWriteFile(const char* path,
                                  const char* data,
                                  int64_t data_size) {
  int temp_file = open(path, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);

  if (temp_file < 0) {
    return false;
  }

  ftruncate(temp_file, 0);

  const char* source = data;
  int64_t to_write = data_size;

  while (to_write > 0) {
    const int to_write_max = static_cast<int>(std::min(
        to_write, static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
    const int bytes_written = write(temp_file, source, to_write_max);
    RecordFileWriteStat(bytes_written);

    if (bytes_written < 0) {
      close(temp_file);
      unlink(path);
      return false;
    }

    source += bytes_written;
    to_write -= bytes_written;
  }

  fsync(temp_file);

  if (close(temp_file)) {
    return false;
  }
  return true;
}

}  // namespace starboard::shared::starboard
