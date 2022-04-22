// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <vector>

#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/file_atomic_replace_write_file.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace {

const char kTempFileSuffix[] = ".temp";

}  // namespace

bool SbFileAtomicReplace(const char* path,
                         const char* data,
                         int64_t data_size) {
  if ((data_size < 0) || ((data_size > 0) && !data)) {
    return false;
  }

  std::vector<char> temp_path(kSbFileMaxPath + 1, 0);

  starboard::strlcpy(temp_path.data(), path, kSbFileMaxPath);
  starboard::strlcat(temp_path.data(), kTempFileSuffix, kSbFileMaxPath);

  if (!::starboard::shared::starboard::SbFileAtomicReplaceWriteFile(
          temp_path.data(), data, data_size)) {
    return false;
  }

  std::wstring path_wstring = starboard::shared::win32::NormalizeWin32Path(
      starboard::shared::win32::CStringToWString(path));
  std::wstring temp_path_wstring = starboard::shared::win32::NormalizeWin32Path(
      starboard::shared::win32::CStringToWString(temp_path.data()));

  // Try a simple move first. It will only succeed when |to_path| doesn't
  // already exist.
  if (MoveFileEx(temp_path_wstring.c_str(), path_wstring.c_str(), 0)) {
    return true;
  }

  // Try the full-blown replace if the move fails, as ReplaceFile will only
  // succeed when |to_path| does exist.
  if (ReplaceFile(path_wstring.c_str(), temp_path_wstring.c_str(), NULL, 0,
                  NULL, NULL)) {
    return true;
  }
  return false;
}
