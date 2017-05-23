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

#include "starboard/system.h"

#include <windows.h>
#include <codecvt>
#include <cstring>
#include <locale>

#include "Pathcch.h"

#include "starboard/directory.h"
#include "starboard/log.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

bool GetExecutablePath(char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  static const int kPathSize = SB_FILE_MAX_PATH;

  wchar_t w_file_path[SB_FILE_MAX_PATH];
  DWORD characters_written =
      GetModuleFileName(NULL, w_file_path, SB_FILE_MAX_PATH);
  if (characters_written == 0) {
    return false;
  }
  PathCchRemoveFileSpec(w_file_path, SB_FILE_MAX_PATH);
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path, SB_FILE_MAX_PATH);
  return SbStringCopy(out_path, utf8_string.c_str(), SB_FILE_MAX_PATH);
}

// Note: This function is only minimally implemented to allow tests to run.
bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  static const int kPathSize = SB_FILE_MAX_PATH;

  char file_path[kPathSize];
  file_path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory: {
      if (!GetExecutablePath(file_path, kPathSize)) {
        return false;
      }
      if (SbStringConcat(file_path, "/content/data", kPathSize) >= kPathSize) {
        return false;
      }
      SbStringCopy(out_path, file_path, path_size);
      break;
    }
    case kSbSystemPathTempDirectory: {
      wchar_t w_file_path[kPathSize];
      w_file_path[0] = '\0';

      DWORD characters_written = GetTempPathW(kPathSize, w_file_path);
      if (characters_written >= (kPathSize - 1)) {
        return false;
      }

      if (characters_written < 1) {
        return false;
      }
      // Remove the last slash, to match other Starboard implementations.
      w_file_path[characters_written - 1] = L'\0';

      std::string utf8_string =
          starboard::shared::win32::wchar_tToUTF8(w_file_path);

      if (SbStringCopy(file_path, utf8_string.c_str(), kPathSize) >=
          kPathSize) {
        return false;
      }
      SbDirectoryCreate(file_path);

      size_t length = SbStringGetLength(file_path);
      if (length < 1 || length > path_size) {
        return false;
      }

      SbStringCopy(out_path, file_path, path_size);
      break;
    }
    case kSbSystemPathExecutableFile: {
      return GetExecutablePath(out_path, path_size);
    }
    // TODO: implement all the other cases.
    default:
      SB_NOTIMPLEMENTED();
      return false;
  }
  return true;
}
