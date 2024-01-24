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

bool SbFileRename(const char* from_path, const char* to_path) {
  using starboard::shared::win32::CStringToWString;
  using starboard::shared::win32::NormalizeWin32Path;

  if ((from_path == nullptr) || *from_path == '\0') {
    return false;
  }

  if ((to_path == nullptr) || *to_path == '\0') {
    return false;
  }

  if (!SbFileExists(from_path)) {
    return false;
  }

  std::wstring from_path_wstring = NormalizeWin32Path(from_path);
  std::wstring to_path_wstring = NormalizeWin32Path(to_path);
  return MoveFileW(from_path_wstring.c_str(), to_path_wstring.c_str());
}
