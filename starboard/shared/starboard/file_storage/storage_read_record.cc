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

#include "starboard/common/storage.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"
#include "starboard/user.h"

int64_t SbStorageReadRecord(SbStorageRecord record,
                            char* out_data,
                            int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !out_data || data_size < 0) {
    return -1;
  }

  if (!SbFileIsValid(record->file)) {
    return -1;
  }

  int64_t total = SbFileSeek(record->file, kSbFileFromEnd, 0);
  if (total > data_size) {
    total = data_size;
  }

  int64_t position = SbFileSeek(record->file, kSbFileFromBegin, 0);
  if (position != 0) {
    return -1;
  }

  char* destination = out_data;
  int64_t to_read = total;
  while (to_read > 0) {
    int to_read_max =
        static_cast<int>(std::min(to_read, static_cast<int64_t>(kSbInt32Max)));
    int bytes_read = SbFileRead(record->file, destination, to_read_max);
    if (bytes_read < 0) {
      return -1;
    }

    destination += bytes_read;
    to_read -= bytes_read;
  }

  return total;
}
