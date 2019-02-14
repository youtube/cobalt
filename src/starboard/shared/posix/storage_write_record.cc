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

#include "starboard/storage.h"

#include <algorithm>

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"
#include "starboard/string.h"

const char kTempFileSuffix[] = ".temp";

bool SbStorageWriteRecord(SbStorageRecord record,
                          const char* data,
                          int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !data || data_size < 0) {
    return false;
  }

#if SB_API_VERSION >= 6
  const char* name = record->name.c_str();
#else
  const char* name = NULL;
#endif
  char original_file_path[SB_FILE_MAX_PATH];
  if (!starboard::shared::starboard::GetUserStorageFilePath(
          record->user, name, original_file_path, SB_FILE_MAX_PATH)) {
    return false;
  }
  char temp_file_path[SB_FILE_MAX_PATH];
  SbStringCopy(temp_file_path, original_file_path, SB_FILE_MAX_PATH);
  SbStringConcat(temp_file_path, kTempFileSuffix, SB_FILE_MAX_PATH);

  SbFileError error;
  SbFile temp_file = SbFileOpen(
      temp_file_path, kSbFileCreateAlways | kSbFileWrite | kSbFileRead, NULL,
      &error);
  if (error != kSbFileOk) {
    return false;
  }

  SbFileTruncate(temp_file, 0);

  const char* source = data;
  int64_t to_write = data_size;
  while (to_write > 0) {
    int to_write_max =
        static_cast<int>(std::min(to_write, static_cast<int64_t>(kSbInt32Max)));
    int bytes_written = SbFileWrite(temp_file, source, to_write_max);
    if (bytes_written < 0) {
      SbFileClose(temp_file);
      SbFileDelete(temp_file_path);
      return false;
    }

    source += bytes_written;
    to_write -= bytes_written;
  }

  SbFileFlush(temp_file);

  if (SbFileIsValid(record->file) && !SbFileClose(record->file)) {
    return false;
  }

  record->file = kSbFileInvalid;

  if (!SbFileDelete(original_file_path)) {
    return false;
  }

  if (rename(temp_file_path, original_file_path) != 0) {
    return false;
  }

  SbFile original_file =
      SbFileOpen(original_file_path, kSbFileOpenOnly, NULL, &error);
  if (error != kSbFileOk) {
    return false;
  }
  SbFileFlush(original_file);

  record->file = temp_file;

  return true;
}
