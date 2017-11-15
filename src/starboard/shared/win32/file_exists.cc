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

#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

bool SbFileExists(const char* path) {
  using starboard::shared::win32::CStringToWString;
  using starboard::shared::win32::IsValidHandle;
  using starboard::shared::win32::NormalizeWin32Path;
  using starboard::shared::win32::PathEndsWith;

  if ((path == nullptr) || (path[0] == '\0')) {
    return false;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);
  // Win32 does not like a trailing "\\" on the path names for directories,
  // so it's needs to be chopped off.
  if (!path_wstring.empty() && (path_wstring.back() == '\\')) {
    path_wstring.pop_back();
  }

  WIN32_FIND_DATA find_data = {0};

  HANDLE search_handle = FindFirstFileExW(
      path_wstring.c_str(), FindExInfoStandard, &find_data,
      FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);

  if (!IsValidHandle(search_handle)) {
    return false;
  }

  FindClose(search_handle);

  return PathEndsWith(path_wstring, find_data.cFileName);
}
