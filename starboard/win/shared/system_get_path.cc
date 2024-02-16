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

#include "starboard/system.h"

#include <windows.h>
// pathcch.h must come after windows.h.
#include <pathcch.h>

#include <codecvt>
#include <cstring>
#include <locale>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace {

using starboard::shared::win32::CreateDirectoryHierarchy;
using starboard::shared::win32::NormalizeWin32Path;

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }

  std::vector<wchar_t> w_file_path(kSbFileMaxPath);
  DWORD characters_written =
      GetModuleFileName(NULL, w_file_path.data(), kSbFileMaxPath);
  if (characters_written < 1) {
    return false;
  }
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path.data());
  if (utf8_string.length() >= path_size) {
    return false;
  }
  return starboard::strlcpy(out_path, utf8_string.c_str(), path_size);
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

  std::vector<wchar_t> w_file_path(kSbFileMaxPath);
  DWORD characters_written =
      GetModuleFileName(NULL, w_file_path.data(), kSbFileMaxPath);
  if (characters_written < 1) {
    return false;
  }
  PathCchRemoveFileSpec(w_file_path.data(), kSbFileMaxPath);
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path.data());
  if (utf8_string.length() >= path_size) {
    return false;
  }
  return starboard::strlcpy(out_path, utf8_string.c_str(), path_size);
}

bool GetRelativeDirectory(const char* relative_path,
                          char* out_path,
                          int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  std::vector<char> file_path(kSbFileMaxPath);
  file_path[0] = '\0';
  if (!GetExecutableDirectory(file_path.data(), path_size)) {
    return false;
  }
  if (starboard::strlcat(file_path.data(), relative_path, kSbFileMaxPath) >=
      path_size) {
    return false;
  }

  if (!CreateDirectoryHierarchy(NormalizeWin32Path(file_path.data()))) {
    return false;
  }
  return starboard::strlcpy(out_path, file_path.data(), path_size);
}

// Places up to |path_size| - 1 characters of the path to the content directory
// in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetContentPath(char* out_path, int path_size) {
  return GetRelativeDirectory("\\content\\data", out_path, path_size);
}

bool GetCachePath(char* out_path, int path_size) {
  return GetRelativeDirectory("\\content\\cache", out_path, path_size);
}

bool CreateAndGetTempPath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  std::vector<wchar_t> w_file_path(kSbFileMaxPath);
  w_file_path[0] = L'\0';

  int64_t characters_written =
      static_cast<int>(GetTempPathW(kSbFileMaxPath, w_file_path.data()));
  if (characters_written >= (path_size + 1) || characters_written < 1) {
    return false;
  }
  // Remove the last slash, to match other Starboard implementations.
  w_file_path[characters_written - 1] = L'\0';

  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(w_file_path.data());

  if (starboard::strlcpy(out_path, utf8_string.c_str(), path_size) >=
      path_size) {
    return false;
  }
  SbDirectoryCreate(out_path);

  size_t length = strlen(out_path);
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
