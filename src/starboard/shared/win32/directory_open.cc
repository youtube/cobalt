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

#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

SbDirectory SbDirectoryOpen(const char* path, SbFileError* out_error) {
  using starboard::shared::win32::CStringToWString;
  using starboard::shared::win32::NormalizeWin32Path;

  if ((path == nullptr) || (path[0] == '\0')) {
    if (out_error) {
      *out_error = kSbFileErrorNotFound;
    }
    return kSbDirectoryInvalid;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);

  if (!starboard::shared::win32::IsAbsolutePath(path_wstring)) {
    if (out_error) {
      *out_error = kSbFileErrorNotFound;
    }
    return kSbDirectoryInvalid;
  }

  HANDLE directory_handle = starboard::shared::win32::OpenFileOrDirectory(
      path, kSbFileOpenOnly | kSbFileRead, nullptr, out_error);

  if (!starboard::shared::win32::IsValidHandle(directory_handle)) {
    return kSbDirectoryInvalid;
  }

  FILE_BASIC_INFO basic_info = {0};
  BOOL basic_info_success = GetFileInformationByHandleEx(
      directory_handle, FileBasicInfo, &basic_info, sizeof(FILE_BASIC_INFO));

  if (!basic_info_success ||
      !(basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    CloseHandle(directory_handle);
    if (out_error) {
      *out_error = kSbFileErrorNotADirectory;
    }
    return kSbDirectoryInvalid;
  }

  return new SbDirectoryPrivate(directory_handle);
}
