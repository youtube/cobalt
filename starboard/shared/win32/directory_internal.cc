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

#include "starboard/shared/win32/directory_internal.h"

#include "starboard/directory.h"

#include <algorithm>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/memory.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

bool HasValidHandle(SbDirectory directory) {
  if (!SbDirectoryIsValid(directory)) {
    return false;
  }
  return directory->HasValidHandle();
}

// This function strips trailing file separators from a directory name.
// For example if the directory name was "C:\\Temp\\\\\\", it would
// strip them, so that the directory name is now to be "C:\\temp".
void TrimExtraFileSeparators(std::wstring* dirname_pointer) {
  SB_DCHECK(dirname_pointer);
  std::wstring& dirname = *dirname_pointer;
  auto new_end = std::find_if_not(
      dirname.rbegin(), dirname.rend(),
      [](wchar_t c) { return c == kSbFileSepChar || c == kSbFileAltSepChar; });
  dirname.erase(new_end.base(), dirname.end());
}

bool IsAbsolutePath(const std::wstring& path) {
  std::vector<wchar_t> full_path(kSbFileMaxPath);
  DWORD full_path_size =
      GetFullPathNameW(path.c_str(), static_cast<DWORD>(full_path.size()),
                       full_path.data(), NULL);
  if (full_path_size == 0) {
    return false;
  }

  int path_size = static_cast<int>(path.size());
  return CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE,
                         path.c_str(), path_size, full_path.data(),
                         full_path_size, NULL, NULL, 0) == CSTR_EQUAL;
}

bool DirectoryExists(const std::wstring& dir_path) {
  if (dir_path.empty()) {
    return false;
  }
  std::wstring norm_dir_path = NormalizeWin32Path(dir_path);
  WIN32_FILE_ATTRIBUTE_DATA attribute_data = {0};
  if (!GetFileAttributesExW(norm_dir_path.c_str(), GetFileExInfoStandard,
                            &attribute_data)) {
    // If this is system folder GetFileAttributesExW returns FALSE
    // but the folder exists
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  return (attribute_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool CreateDirectoryHierarchy(const std::wstring& wfull_path) {
  if (DirectoryExistsOrCreated(wfull_path)) {
    return true;
  }
  const wchar_t kPathSeparators[] = {static_cast<wchar_t>(kSbFileSepChar),
                                     static_cast<wchar_t>(kSbFileAltSepChar)};
  size_t path_end = 0;
  do {
    path_end = wfull_path.find_first_of(kPathSeparators, path_end,
                                        SB_ARRAY_SIZE(kPathSeparators));
    if (path_end == std::wstring::npos) {
      path_end = wfull_path.size();
    }
    std::wstring temp_path = wfull_path.substr(0, path_end);
    DirectoryExistsOrCreated(temp_path);
  } while (path_end < wfull_path.size());

  return DirectoryExistsOrCreated(wfull_path);
}

bool DirectoryExistsOrCreated(const std::wstring& wpath) {
  return DirectoryExists(wpath) || CreateDirectoryW(wpath.c_str(), NULL);
}

bool ClearOrDeleteDirectory(const std::wstring& wpath, bool deleteIt) {
  if (!DirectoryExists(wpath)) {
    return true;
  }
  std::wstring searchPath = wpath + L"\\*";

  WIN32_FIND_DATAW ffd;
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &ffd);
  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }
  bool has_error = false;
  do {
    std::wstring name = ffd.cFileName;

    // skip"." and ".."
    if (name == L"." || name == L"..")
      continue;

    std::wstring fullPath = wpath + L"\\" + name;

    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // recursively delete subdirectory
      if (!ClearOrDeleteDirectory(fullPath, true)) {
        has_error |= true;
      }
    } else {
      // remove read-only attribute if any
      SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
      if (!DeleteFileW(fullPath.c_str())) {
        std::string fullPathA =
            ::starboard::shared::win32::wchar_tToUTF8(fullPath.c_str());
        SB_LOG(INFO) << "Unable to delete file " << fullPathA;
        has_error |= true;
      }
    }
  } while (FindNextFile(hFind, &ffd) != 0);

  FindClose(hFind);

  // remove root folder
  if (deleteIt) {
    has_error |= RemoveDirectoryW(wpath.c_str()) == 0;
  }
  return !has_error;
}
}  // namespace win32
}  // namespace shared
}  // namespace starboard
