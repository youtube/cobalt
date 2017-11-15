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

#include "starboard/shared/win32/file_internal.h"

#include <windows.h>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace sbwin32 = starboard::shared::win32;

namespace starboard {
namespace shared {
namespace win32 {

namespace {
const char kUnixSep[] = "/";
const char kWin32Sep[] = "\\";
const wchar_t kUnixSepW[] = L"/";
const wchar_t kWin32SepW[] = L"\\";

bool IsPathNormalized(const std::string& string) {
  return string.find(kUnixSep) == std::string::npos;
}

bool IsPathNormalized(const std::wstring& string) {
  return string.find(kUnixSepW) == std::wstring::npos;
}

std::string NormalizePathSeperator(std::string str) {
  size_t start_pos = 0;
  while ((start_pos = str.find(kUnixSep, start_pos)) != std::string::npos) {
    str.replace(start_pos, sizeof(kUnixSep) - 1, kWin32Sep);
    start_pos += sizeof(kWin32Sep) - 1;
  }
  return str;
}

std::wstring NormalizePathSeperator(std::wstring str) {
  size_t start_pos = 0;
  while ((start_pos = str.find(kUnixSepW, start_pos)) != std::wstring::npos) {
    str.replace(start_pos, sizeof(kUnixSepW) / 2 - 1, kWin32SepW);
    start_pos += sizeof(kWin32SepW) / 2 - 1;
  }
  return str;
}

bool StringCanNarrow(const std::wstring& str) {
  for (wchar_t value : str) {
    char narrow_val = static_cast<char>(value);
    if (value != narrow_val) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::wstring NormalizeWin32Path(std::string str) {
  return NormalizeWin32Path(CStringToWString(str.c_str()));
}

std::wstring NormalizeWin32Path(std::wstring str) {
  return NormalizePathSeperator(str);
}

HANDLE OpenFileOrDirectory(const char* path,
                           int flags,
                           bool* out_created,
                           SbFileError* out_error) {
  const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;

  DWORD creation_disposition = 0;
  if (flags & kSbFileCreateOnly) {
    SB_DCHECK(!creation_disposition);
    SB_DCHECK(!(flags & kSbFileCreateAlways));
    creation_disposition = CREATE_NEW;
  }

  if (out_created) {
    *out_created = false;
  }

  if (flags & kSbFileCreateAlways) {
    SB_DCHECK(!creation_disposition);
    SB_DCHECK(!(flags & kSbFileCreateOnly));
    creation_disposition = CREATE_ALWAYS;
  }

  if (flags & kSbFileOpenTruncated) {
    SB_DCHECK(!creation_disposition);
    SB_DCHECK(flags & kSbFileWrite);
    creation_disposition = TRUNCATE_EXISTING;
  }

  if (flags & kSbFileOpenOnly) {
    SB_DCHECK(!(flags & kSbFileOpenAlways));
    creation_disposition = OPEN_EXISTING;
  }

  if (flags & kSbFileOpenAlways) {
    SB_DCHECK(!(flags & kSbFileOpenOnly));
    creation_disposition = OPEN_ALWAYS;
  }

  if (!creation_disposition && !(flags & kSbFileOpenOnly) &&
      !(flags & kSbFileOpenAlways)) {
    SB_NOTREACHED();
    errno = ENOTSUP;
    if (out_error) {
      *out_error = kSbFileErrorFailed;
    }

    return kSbFileInvalid;
  }

  DWORD desired_access = 0;
  if (flags & kSbFileRead) {
    desired_access |= GENERIC_READ;
  }

  const bool open_file_in_write_mode = flags & kSbFileWrite;
  if (open_file_in_write_mode) {
    desired_access |= GENERIC_WRITE;
  }

  // TODO: Support asynchronous IO, if necessary.
  SB_DCHECK(!(flags & kSbFileAsync));

  SB_DCHECK(desired_access != 0) << "Invalid permission flag.";

  std::wstring path_wstring = starboard::shared::win32::CStringToWString(path);

  CREATEFILE2_EXTENDED_PARAMETERS create_ex_params = {0};
  // Enabling |FILE_FLAG_BACKUP_SEMANTICS| allows us to figure out if the path
  // is a directory.
  create_ex_params.dwFileFlags = FILE_FLAG_BACKUP_SEMANTICS;
  create_ex_params.dwFileFlags |= FILE_FLAG_POSIX_SEMANTICS;
  create_ex_params.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
  create_ex_params.dwSecurityQosFlags = SECURITY_ANONYMOUS;
  create_ex_params.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);

  HANDLE file_handle =
      CreateFile2(path_wstring.c_str(), desired_access, share_mode,
                  creation_disposition, &create_ex_params);

  if (out_created) {
    if (flags & (kSbFileCreateAlways | kSbFileCreateOnly)) {
      *out_created = starboard::shared::win32::IsValidHandle(file_handle);
    }

    if ((creation_disposition == OPEN_ALWAYS) && (open_file_in_write_mode)) {
      *out_created = (GetLastError() != ERROR_ALREADY_EXISTS);
    }
  }

  const DWORD last_error = GetLastError();

  if (out_error) {
    if (starboard::shared::win32::IsValidHandle(file_handle)) {
      *out_error = kSbFileOk;
    } else {
      switch (last_error) {
        case ERROR_ACCESS_DENIED:
          *out_error = kSbFileErrorAccessDenied;
          break;
        case ERROR_FILE_EXISTS:
          *out_error = kSbFileErrorAccessDenied;
          break;
        case ERROR_FILE_NOT_FOUND:
          *out_error = kSbFileErrorNotFound;
          break;
        default:
          *out_error = kSbFileErrorFailed;
      }
    }
  }

  return file_handle;
}

bool DirectoryExists(const std::wstring& dir_path) {
  if (dir_path.empty()) {
    return false;
  }
  std::wstring norm_dir_path = NormalizeWin32Path(dir_path);
  WIN32_FILE_ATTRIBUTE_DATA attribute_data = {0};
  if (!GetFileAttributesExW(norm_dir_path.c_str(), GetFileExInfoStandard,
                            &attribute_data)) {
    return false;
  }
  return (attribute_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
