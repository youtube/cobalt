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

#include "starboard/directory.h"

#include <vector>
#include <windows.h>

#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::DirectoryExists;
using starboard::shared::win32::IsAbsolutePath;
using starboard::shared::win32::NormalizeWin32Path;
using starboard::shared::win32::TrimExtraFileSeparators;

namespace {

bool DirectoryExistsOrCreated(const std::wstring& wpath) {
  return DirectoryExists(wpath) || CreateDirectoryW(wpath.c_str(), NULL);
}

// Example:
//  Input: L"C:\\User\\Dir"
//  Output: { L"C:\\User\\Dir", L"C:\\User", L"C:" }
std::vector<std::wstring> CreateParentDirs(std::wstring path) {
  std::vector<std::wstring> output;
  output.push_back(path);
  std::size_t pos = path.rfind(L"\\");
  while (pos != std::wstring::npos) {
    output.push_back(path.substr(0, pos));
    if (pos == 0) {
      break;
    }
    pos = path.rfind(L"\\", pos - 1);
  }
  return output;
}

// Directory hierarchy is created from tip down to root. This is necessary
// because UWP has issues with bottom up directory creation due to permissions.
bool CreateDirectoryHiearchy(const std::wstring& wfull_path) {
  std::vector<std::wstring> parent_dirs = CreateParentDirs(wfull_path);

  for (size_t i = 0; i < parent_dirs.size(); ++i) {
    if (DirectoryExists(parent_dirs[i])) {
      break;
    }
    BOOL directory_created = CreateDirectoryW(parent_dirs[i].c_str(), NULL);
    if (directory_created) {
      // Move forward now and create child directories all the way to the tip.
      for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
        const std::wstring& cur_path = parent_dirs[static_cast<size_t>(j)];
        if (DirectoryExistsOrCreated(cur_path)) {
          continue;
        }
      }
      break;
    }
  }
  return DirectoryExistsOrCreated(wfull_path);
}

}  // namespace.

bool SbDirectoryCreate(const char* path) {
  if ((path == nullptr) || (path[0] == '\0')) {
    return false;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);
  TrimExtraFileSeparators(&path_wstring);

  if (!IsAbsolutePath(path_wstring)) {
    return false;
  }

  if (DirectoryExistsOrCreated(path_wstring)) {
    return true;
  }

  return CreateDirectoryHiearchy(path_wstring);
}
