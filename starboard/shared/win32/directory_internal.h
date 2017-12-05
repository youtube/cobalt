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

#ifndef STARBOARD_SHARED_WIN32_DIRECTORY_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_DIRECTORY_INTERNAL_H_

#include "starboard/directory.h"

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/win32/file_internal.h"

#pragma warning(push)

// SbFilePrivate is defined as a struct, but for windows implementation
// enough functionality has been added so that it warrants being a class
// per Google's C++ style guide.  This mismatch causes the Microsoft's compiler
// to generate a warning.
#pragma warning(disable : 4099)

class SbDirectoryPrivate {
 public:
  explicit SbDirectoryPrivate(HANDLE handle) : directory_handle(handle) {}

  bool HasValidHandle() const {
    return starboard::shared::win32::IsValidHandle(directory_handle);
  }

  HANDLE directory_handle;
  std::deque<std::string> next_directory_entries;

  // SbDirectoryPrivate is neither copyable nor movable.
  SbDirectoryPrivate(const SbDirectoryPrivate&) = delete;
  SbDirectoryPrivate& operator=(const SbDirectoryPrivate&) = delete;
};
#pragma warning(pop)

namespace starboard {
namespace shared {
namespace win32 {

bool HasValidHandle(SbDirectory directory);

// This function strips trailing file separators from a directory name.
// For example if the directory name was "C:\\Temp\\\\\\", it would
// strip them, so that the directory name is now to be "C:\\temp".
void TrimExtraFileSeparators(std::wstring* dirname_pointer);

bool IsAbsolutePath(const std::wstring& path);

bool DirectoryExists(const std::wstring& dir_path);

// Directory hierarchy is created from tip down to root. This is necessary
// because UWP has issues with bottom up directory creation due to permissions.
bool CreateDirectoryHiearchy(const std::wstring& wfull_path);

bool DirectoryExistsOrCreated(const std::wstring& wpath);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_DIRECTORY_INTERNAL_H_
