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
#if SB_API_VERSION < 17
#include "starboard/directory.h"

#include <windows.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace {
// One of the entries of FILE_ID_BOTH_DIR_INFO is a file path, so make the
// buffer at-least one path big.
const std::size_t kDirectoryInfoBufferSize =
    kSbFileMaxPath + sizeof(FILE_ID_BOTH_DIR_INFO);

std::deque<std::string> GetDirectoryEntries(HANDLE directory_handle) {
  // According to
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364226(v=vs.85).aspx,
  // FILE_ID_BOTH_DIR_INFO must be aligned on a DWORDLONG boundary.
  alignas(sizeof(DWORDLONG)) std::vector<char> directory_info_buffer(
      kDirectoryInfoBufferSize);

  std::deque<std::string> entries;
  BOOL directory_info_success = GetFileInformationByHandleEx(
      directory_handle, FileIdBothDirectoryInfo, directory_info_buffer.data(),
      static_cast<int>(directory_info_buffer.size()));

  if (!directory_info_success) {
    return entries;
  }

  const char* directory_info_pointer = directory_info_buffer.data();
  DWORD next_entry_offset = 0;

  do {
    auto directory_info =
        reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(directory_info_pointer);

    // FileName is in Unicode, so divide by 2 to get the real length.
    DWORD number_characters_in_filename = directory_info->FileNameLength / 2;
    std::string ascii_path = starboard::shared::win32::wchar_tToUTF8(
        directory_info->FileName, number_characters_in_filename);
    SB_DCHECK(ascii_path.size() == number_characters_in_filename);
    bool is_dotted_directory =
        !ascii_path.compare(".") || !ascii_path.compare("..");
    if (!is_dotted_directory) {
      entries.emplace_back(std::move(ascii_path));
    }
    next_entry_offset = directory_info->NextEntryOffset;
    directory_info_pointer += next_entry_offset;
  } while (next_entry_offset != 0);

  return entries;
}

}  // namespace

bool SbDirectoryGetNext(SbDirectory directory,
                        char* out_entry,
                        size_t out_entry_size) {
  if (!SbDirectoryIsValid(directory) || out_entry == nullptr ||
      out_entry_size < kSbFileMaxName) {
    return false;
  }

  auto& next_directory_entries = directory->next_directory_entries;
  if (next_directory_entries.empty()) {
    next_directory_entries = GetDirectoryEntries(directory->directory_handle);
  }

  if (next_directory_entries.empty()) {
    return false;
  }

  bool success = true;
  if (starboard::strlcpy(out_entry, next_directory_entries.rbegin()->c_str(),
                         static_cast<int>(out_entry_size)) >= out_entry_size) {
    success = false;
  }
  directory->next_directory_entries.pop_back();
  return success;
}
#endif  // SB_API_VERSION < 17
