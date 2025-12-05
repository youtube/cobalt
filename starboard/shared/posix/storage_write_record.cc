// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/storage.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"

const char kTempFileSuffix[] = ".temp";

bool SbStorageWriteRecord(SbStorageRecord record,
                          const char* data,
                          int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !data || data_size < 0) {
    return false;
  }

  const char* name = record->name.c_str();
  std::vector<char> original_file_path(kSbFileMaxPath);
  if (!starboard::GetStorageFilePath(name, original_file_path.data(),
                                     kSbFileMaxPath)) {
    return false;
  }

  std::vector<char> temp_file_path(kSbFileMaxPath + 1, 0);
  starboard::strlcpy(temp_file_path.data(), original_file_path.data(),
                     kSbFileMaxPath);
  starboard::strlcat(temp_file_path.data(), kTempFileSuffix, kSbFileMaxPath);

  int temp_file = open(temp_file_path.data(), O_CREAT | O_TRUNC | O_RDWR,
                       S_IRUSR | S_IWUSR);
  if (!starboard::IsValid(temp_file)) {
    return false;
  }

  ftruncate(temp_file, 0);

  const char* source = data;
  int64_t to_write = data_size;
  while (to_write > 0) {
    int to_write_max = static_cast<int>(std::min(
        to_write, static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
    int bytes_written = write(temp_file, source, to_write_max);
    if (bytes_written < 0) {
      close(temp_file);
      unlink(temp_file_path.data());
      return false;
    }

    source += bytes_written;
    to_write -= bytes_written;
  }

  fsync(temp_file);

  if (starboard::IsValid(record->file) && (close(record->file) < 0)) {
    close(temp_file);
    unlink(temp_file_path.data());
    return false;
  }

  record->file = -1;

  if (unlink(original_file_path.data()) ||
      (rename(temp_file_path.data(), original_file_path.data()) != 0)) {
    close(temp_file);
    unlink(temp_file_path.data());
    return false;
  }

  fsync(temp_file);

  record->file = temp_file;

  return true;
}
