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

#ifndef STARBOARD_SHARED_WIN32_FILE_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_FILE_INTERNAL_H_

#include <wtypes.h>

#include <cwchar>  // This file included for std::wcslen.
#include <string>

#include "starboard/file.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace win32 {

inline bool IsValidHandle(HANDLE handle) {
  return handle != INVALID_HANDLE_VALUE;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#pragma warning(push)

// SbFilePrivate is defined as a struct, but for windows implementation
// enough functionality has been added so that it warrants being a class
// per Google's C++ style guide.  This mismatch causes the Microsoft's compiler
// to generate a warning.
#pragma warning(disable : 4099)

class SbFilePrivate {
 public:
  explicit SbFilePrivate(HANDLE handle) : file_handle(handle) {}

  bool HasValidHandle() const {
    return starboard::shared::win32::IsValidHandle(file_handle);
  }

  HANDLE file_handle;

  // SbFilePrivate is neither copyable nor movable.
  SbFilePrivate(const SbFilePrivate&) = delete;
  SbFilePrivate& operator=(const SbFilePrivate&) = delete;
};
#pragma warning(pop)

namespace starboard {
namespace shared {
namespace win32 {

inline bool HasValidHandle(SbFile file) {
  if (!SbFileIsValid(file)) {
    return false;
  }

  return file->HasValidHandle();
}

inline bool PathEndsWith(const std::wstring& path, const wchar_t* filename) {
  size_t filename_length = std::wcslen(filename);
  if (filename_length > path.size()) {
    return false;
  }

  size_t path_offset = path.size() - filename_length;

  return wcscmp(path.c_str() + path_offset, filename) == 0;
}

// Path's from cobalt use "/" as a path separator. This function will
// replace all of the "/" with "\".
std::wstring NormalizeWin32Path(std::string str);
std::wstring NormalizeWin32Path(std::wstring str);

HANDLE OpenFileOrDirectory(const char* path,
                           int flags,
                           bool* out_created,
                           SbFileError* out_error);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_FILE_INTERNAL_H_
