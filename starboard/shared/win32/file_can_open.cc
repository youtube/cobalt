// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

bool SbFileCanOpen(const char* path, int flags) {
  if ((path == nullptr) || (path[0] == '\0')) {
    return false;
  }

  bool can_read = flags & kSbFileRead;
  bool can_write = flags & kSbFileWrite;
  if (!can_read && !can_write) {
    return false;
  }

  std::wstring path_wstring = starboard::shared::win32::CStringToWString(path);
  WIN32_FIND_DATA find_data = {0};

  HANDLE search_handle = FindFirstFileExW(
      path_wstring.c_str(), FindExInfoStandard, &find_data,
      FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);
  if (!starboard::shared::win32::IsValidHandle(search_handle)) {
    return false;
  }

  bool can_open = true;

  if (((find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) && can_write) ||
      !starboard::shared::win32::PathEndsWith(path_wstring, find_data.cFileName)) {
    can_open = false;
  }

  FindClose(search_handle);

  return can_open;
}
