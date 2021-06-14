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

#include "starboard/common/storage.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

const char kTempFileSuffix[] = ".temp";

bool SbStorageWriteRecord(SbStorageRecord record,
                          const char* data,
                          int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !data || data_size < 0) {
    return false;
  }

  const char* name = record->name.c_str();
  std::vector<char> original_file_path(kSbFileMaxPath);
  if (!starboard::shared::starboard::GetUserStorageFilePath(
          record->user, name, original_file_path.data(), kSbFileMaxPath)) {
    return false;
  }

  std::vector<char> temp_file_path(kSbFileMaxPath + 1, 0);
  starboard::strlcpy(temp_file_path.data(), original_file_path.data(),
                     kSbFileMaxPath);
  starboard::strlcat(temp_file_path.data(), kTempFileSuffix, kSbFileMaxPath);

  SbFileError error;
  SbFile temp_file = SbFileOpen(
      temp_file_path.data(), kSbFileCreateAlways | kSbFileWrite | kSbFileRead,
      NULL, &error);
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
      SbFileDelete(temp_file_path.data());
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

  if (!SbFileClose(temp_file)) {
    return false;
  }

  std::wstring original_path_wstring =
      starboard::shared::win32::NormalizeWin32Path(
          starboard::shared::win32::CStringToWString(
              original_file_path.data()));
  std::wstring temp_path_wstring = starboard::shared::win32::NormalizeWin32Path(
      starboard::shared::win32::CStringToWString(temp_file_path.data()));
  if (ReplaceFileW(original_path_wstring.c_str(), temp_path_wstring.c_str(),
                   NULL, 0, NULL, NULL) == 0) {
    return false;
  }
  SbFile new_record_file =
      SbFileOpen(original_file_path.data(),
                 kSbFileOpenOnly | kSbFileWrite | kSbFileRead, NULL, NULL);
  record->file = new_record_file;

  return true;
}
