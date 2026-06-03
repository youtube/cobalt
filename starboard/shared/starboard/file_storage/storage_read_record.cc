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

// clang-format off
#include "starboard/common/storage.h"
// clang-format on

#include <unistd.h>

#include <algorithm>
#include <limits>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"

int64_t SbStorageReadRecord(SbStorageRecord record,
                            char* out_data,
                            int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !out_data || data_size < 0) {
    return -1;
  }

  if (!starboard::IsValid(record->file)) {
    return -1;
  }

  int64_t total = lseek(record->file, 0, SEEK_END);
  if (total > data_size) {
    total = data_size;
  }

  int64_t position = lseek(record->file, 0, SEEK_SET);
  if (position != 0) {
    return -1;
  }

  char* destination = out_data;
  int64_t to_read = total;
  while (to_read > 0) {
    int to_read_max = static_cast<int>(std::min(
        to_read, static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
    int bytes_read = read(record->file, destination, to_read_max);
    if (bytes_read < 0) {
      return -1;
    }

    destination += bytes_read;
    to_read -= bytes_read;
  }

  return total;
}
