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

#include "starboard/log.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/time_utils.h"
#include "starboard/shared/win32/wchar_utils.h"

bool SbFileGetPathInfo(const char* path, SbFileInfo* out_info) {
  using starboard::shared::win32::CStringToWString;
  using starboard::shared::win32::NormalizeWin32Path;

  if (!path || path[0] == '\0' || !out_info) {
    return false;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);

  WIN32_FILE_ATTRIBUTE_DATA attribute_data = {0};
  if (!GetFileAttributesExW(path_wstring.c_str(), GetFileExInfoStandard,
                            &attribute_data)) {
    return false;
  }

  out_info->size = static_cast<int64_t>(attribute_data.nFileSizeHigh) << 32 |
                   attribute_data.nFileSizeLow;
  SB_DCHECK(out_info->size >= 0);

  using starboard::shared::win32::ConvertFileTimeToSbTime;

  out_info->creation_time =
      ConvertFileTimeToSbTime(attribute_data.ftCreationTime);
  out_info->last_accessed =
      ConvertFileTimeToSbTime(attribute_data.ftLastAccessTime);
  out_info->last_modified =
      ConvertFileTimeToSbTime(attribute_data.ftLastWriteTime);

  out_info->is_symbolic_link = false;
  out_info->is_directory =
      (attribute_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

  return true;
}
