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
#include <codecvt>
#include <cstring>
#include <locale>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::platformStringToString;
using starboard::shared::win32::wchar_tToUTF8;

namespace {

std::string GetBinaryName() {
  const size_t kMaxModuleNameSize = kSbFileMaxName;
  std::vector<wchar_t> buffer(kMaxModuleNameSize);
  DWORD result = GetModuleFileName(NULL, buffer.data(), buffer.size());
  std::string full_binary_path;
  if (result == 0) {
    full_binary_path = "unknown";
  } else {
    full_binary_path = wchar_tToUTF8(buffer.data(), result).c_str();
  }

  std::string::size_type index = full_binary_path.rfind(kSbFileSepChar);
  if (index == std::string::npos) {
    return full_binary_path;
  }

  return full_binary_path.substr(index + 1);
}

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  auto folder =
      Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
  if (folder->Length() >= static_cast<unsigned int>(path_size) ||
      folder->Length() < 1) {
    return false;
  }
  auto app_name = Windows::ApplicationModel::Package::Current->Id->Name;
  if (app_name->Length() >= static_cast<unsigned int>(path_size) ||
      app_name->Length() < 1) {
    return false;
  }

  std::string utf8_string =
      (platformStringToString(folder) + "\\" + GetBinaryName());

  if (utf8_string.length() > static_cast<unsigned int>(path_size)) {
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
  auto folder =
      Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
  if (folder->Length() >= static_cast<unsigned int>(path_size) ||
      folder->Length() < 1) {
    return false;
  }
  std::string utf8_string =
      starboard::shared::win32::wchar_tToUTF8(folder->Data(), folder->Length());
  return starboard::strlcpy(out_path, utf8_string.c_str(), path_size);
}

// Places up to |path_size| - 1 characters of the path to the content directory
// in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetContentPath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  std::vector<char> file_path(kSbFileMaxPath);
  file_path[0] = '\0';
  if (!GetExecutableDirectory(file_path.data(), path_size)) {
    return false;
  }
  if (starboard::strlcat(file_path.data(), "\\content\\data", kSbFileMaxPath) >=
      path_size) {
    return false;
  }
  return starboard::strlcpy(out_path, file_path.data(), path_size);
}

// Places up to |path_size| - 1 characters of the path to the temporary
// directory in |out_path|, ensuring it is NULL-terminated, and creates the
// directory if necessary. Returns success status. The result being greater than
// |path_size| - 1 characters is a failure. |out_path| may be written to in
// unsuccessful cases.
bool CreateAndGetTempPath(char* out_path, int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }
  std::vector<wchar_t> w_file_path(kSbFileMaxPath);
  w_file_path[0] = L'\0';

  DWORD characters_written = GetTempPathW(kSbFileMaxPath, w_file_path.data());
  if (static_cast<int>(characters_written) >= (path_size + 1) ||
      characters_written < 1) {
    return false;
  }
  // Remove the last slash, to match other Starboard implementations.
  w_file_path[characters_written - 1] = L'\0';

  std::string utf8_string = wchar_tToUTF8(w_file_path.data());

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

// Places up to |path_size| - 1 characters of the path to the directory in
// |out_path|, ensuring it is NULL-terminated, and creates the directory if
// necessary. Returns success status. The result being greater than |path_size|
// - 1 characters is a failure. |out_path| may be written to in unsuccessful
// cases.
bool CreateAndGetApplicationDataFolder(Windows::Storage::StorageFolder ^ folder,
                                       char* out_path,
                                       int path_size) {
  if (!out_path || (path_size <= 0)) {
    return false;
  }

  // The caller expects that the output string will only be written if when
  // true is returned. Therefore we have to buffer the string in case there is a
  // false condition, such as small memory input size to hold the output
  // parameter.
  std::vector<char> out_path_copy(kSbFileMaxPath, 0);
  std::string local_path = platformStringToString(folder->Path);
  if (starboard::strlcpy(out_path_copy.data(), local_path.c_str(),
                         out_path_copy.size()) >= out_path_copy.size()) {
    return false;
  }

  size_t length = strlen(out_path_copy.data());
  if (length < 1 || length > out_path_copy.size()) {
    return false;
  }

  // The copy is verified, transfer it over to out_path.
  size_t len = std::min<size_t>(path_size, kSbFileMaxPath);
  starboard::strlcpy(out_path, out_path_copy.data(), len);

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
      // We send debug output to the cache directory instead of the temp
      // directory, because the temp directory is removed as soon as the
      // application exits, while also a file can not be opened or copied while
      // it is still open for writing. Using the cache directory makes it
      // possible to retrieve the file contents after the application exits.
      return SbSystemGetPath(kSbSystemPathCacheDirectory, out_path, path_size);
    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);
    case kSbSystemPathStorageDirectory:
      return CreateAndGetApplicationDataFolder(
          Windows::Storage::ApplicationData::Current->LocalFolder, out_path,
          path_size);
    case kSbSystemPathTempDirectory:
      // Note: This temp directory is removed as soon as the application exits.
      return CreateAndGetTempPath(out_path, path_size);
    case kSbSystemPathCacheDirectory:
      return CreateAndGetApplicationDataFolder(
          Windows::Storage::ApplicationData::Current->LocalCacheFolder,
          out_path, path_size);
    // TODO: implement all the other cases.
    default:
      SB_NOTIMPLEMENTED();
      return false;
  }
}
