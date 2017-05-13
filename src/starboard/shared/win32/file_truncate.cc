// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/file.h"

#include <windows.h>

#include <algorithm>

#include "starboard/log.h"
#include "starboard/shared/win32/file_internal.h"

namespace {
static const char k4KZeroPage[4 * 1024] = {0};

bool WriteZerosToFile(HANDLE file_handle,
                      LARGE_INTEGER begin,
                      LARGE_INTEGER end) {
  SB_DCHECK(starboard::shared::win32::IsValidHandle(file_handle));
  int64_t bytes_left_to_write = end.QuadPart - begin.QuadPart;
  if (bytes_left_to_write <= 0) {
    return true;
  }

  // Set the file pointer to |begin|.
  if (!SetFilePointerEx(file_handle, begin, NULL, FILE_BEGIN)) {
    return false;
  }

  // Write from zeros from |begin| to |end|.
  while (bytes_left_to_write > 0) {
    int64_t bytes_to_write =
        std::min<int64_t>(SB_ARRAY_SIZE(k4KZeroPage), bytes_left_to_write);
    SB_DCHECK(bytes_to_write <= kSbInt32Max);

    DWORD bytes_written = 0;
    if (!WriteFile(file_handle, k4KZeroPage, static_cast<int>(bytes_to_write),
                   &bytes_written, NULL)) {
      return false;
    }

    bytes_left_to_write -= bytes_written;
  }

  return true;
}

}  // namespace

bool SbFileTruncate(SbFile file, int64_t length) {
  if (!starboard::shared::win32::HasValidHandle(file) || length < 0) {
    return false;
  }

  HANDLE file_handle = file->file_handle;

  // Get current position.
  LARGE_INTEGER current_position = {0};
  BOOL success =
      SetFilePointerEx(file_handle, {0}, &current_position, FILE_CURRENT);

  if (!success) {
    return false;
  }

  bool return_value = false;
  do {
    LARGE_INTEGER old_eof = {0};
    if (!SetFilePointerEx(file_handle, {0}, &old_eof, FILE_END)) {
      break;
    }

    LARGE_INTEGER new_eof = {0};
    new_eof.QuadPart = length;
    if (!SetFilePointerEx(file_handle, new_eof, NULL, FILE_BEGIN)) {
      break;
    }

    if (!SetEndOfFile(file_handle)) {
      break;
    }

    WriteZerosToFile(file_handle, old_eof, new_eof);
    return_value = true;
  } while (0);

  // Set the file pointer position back where it was.
  SetFilePointerEx(file_handle, current_position, NULL, FILE_BEGIN);

  return return_value;
}
