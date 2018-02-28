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
// pathcch.h must come after windows.h.
#include <pathcch.h>

#include <codecvt>
#include <cstring>
#include <locale>

#include "starboard/directory.h"
#include "starboard/log.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

namespace {

using starboard::shared::win32::CreateDirectoryHiearchy;
using starboard::shared::win32::NormalizeWin32Path;

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }

  wchar_t w_file_path[SB_FILE_MAX_PATH];
  DWORD characters_written =
      GetModuleFileName(NULL, w_file_path, SB_FILE_MAX_PATH);
  if (characters_written < 1) {
    return false;
  }
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path);
  if (utf8_string.length() >= path_size) {
    return false;
  }
  return SbStringCopy(out_path, utf8_string.c_str(), path_size);
}

// Places up to |path_size| - 1 characters of the path to the directory
// containing the current executable in |out_path|, ensuring it is
// NULL-terminated. Returns success status. The result being greater than
// |path_size| - 1 characters is a failure. |out_path| may be written to in
// unsuccessful cases.
bool GetExecutableDirectory(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }

  wchar_t w_file_path[SB_FILE_MAX_PATH];
  DWORD characters_written =
      GetModuleFileName(NULL, w_file_path, SB_FILE_MAX_PATH);
  if (characters_written < 1) {
    return false;
  }
  PathCchRemoveFileSpec(w_file_path, SB_FILE_MAX_PATH);
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path);
  if (utf8_string.length() >= path_size) {
    return false;
  }
  return SbStringCopy(out_path, utf8_string.c_str(), path_size);
}

bool GetRelativeDirectory(const char* relative_path,
                          char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  char file_path[SB_FILE_MAX_PATH];
  file_path[0] = '\0';
  if (!GetExecutableDirectory(file_path, path_size)) {
    return false;
  }
  if (SbStringConcat(file_path, relative_path, SB_FILE_MAX_PATH) >=
      path_size) {
    return false;
  }

  if (!CreateDirectoryHiearchy(NormalizeWin32Path(file_path))) {
    return false;
  }
  return SbStringCopy(out_path, file_path, path_size);
}

// Places up to |path_size| - 1 characters of the path to the content directory
// in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetContentPath(char* out_path, int path_size) {
  return GetRelativeDirectory("\\content\\data", out_path, path_size);
}

bool GetCachePath(char* out_path, int path_size) {
  return GetRelativeDirectory("\\content\\cache",
                              out_path, path_size);
}

bool CreateAndGetTempPath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  wchar_t w_file_path[SB_FILE_MAX_PATH];
  w_file_path[0] = L'\0';

  int64_t characters_written =
      static_cast<int>(GetTempPathW(SB_FILE_MAX_PATH, w_file_path));
  if (characters_written >= (path_size + 1) || characters_written < 1) {
    return false;
  }
  // Remove the last slash, to match other Starboard implementations.
  w_file_path[characters_written - 1] = L'\0';

  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path);

  if (SbStringCopy(out_path, utf8_string.c_str(), path_size) >= path_size) {
    return false;
  }
  SbDirectoryCreate(out_path);

  size_t length = SbStringGetLength(out_path);
  if (length < 1 || length > path_size) {
    return false;
  }
  return true;
}

}  // namespace

// Note: This function is only minimally implemented to allow tests to run.
bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }

  switch (path_id) {
    case kSbSystemPathContentDirectory:
      return GetContentPath(out_path, path_size);
    case kSbSystemPathDebugOutputDirectory:
      return SbSystemGetPath(kSbSystemPathTempDirectory, out_path, path_size);
    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);
    case kSbSystemPathTempDirectory:
      return CreateAndGetTempPath(out_path, path_size);
    case kSbSystemPathCacheDirectory:
      return GetCachePath(out_path, path_size);
    case kSbSystemPathFontConfigurationDirectory:
      return false;
    case kSbSystemPathFontDirectory:
      return false;
    // TODO: implement all the other cases.
    default:
      SB_NOTIMPLEMENTED();
      return false;
  }
}
