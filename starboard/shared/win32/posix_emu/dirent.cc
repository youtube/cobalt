// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <dirent.h>

#include <fcntl.h>
#include <windows.h>
#include <map>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/types.h"

struct CriticalSection {
  CriticalSection() { InitializeCriticalSection(&critical_section_); }
  CRITICAL_SECTION critical_section_;
};

static int gen_fd() {
  static int fd = 100;
  fd++;
  if (fd == 0x7FFFFFFF) {
    fd = 100;
  }
  return fd;
}

static std::map<int, std::deque<std::string>>* directory_map = nullptr;
static CriticalSection g_critical_section;

static int handle_db_put(std::deque<std::string> next_directory_entry) {
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (directory_map == nullptr) {
    directory_map = new std::map<int, std::deque<std::string>>();
  }

  int fd = gen_fd();
  // Go through the map and make sure there isn't duplicated index
  // already.
  while (directory_map->find(fd) != directory_map->end()) {
    fd = gen_fd();
  }

  directory_map->insert({fd, next_directory_entry});

  LeaveCriticalSection(&g_critical_section.critical_section_);
  return fd;
}

static std::deque<std::string> handle_db_get(int fd, bool erase) {
  std::deque<std::string> empty_deque;
  if (fd < 0) {
    return empty_deque;
  }
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (directory_map == nullptr) {
    LeaveCriticalSection(&g_critical_section.critical_section_);
    return empty_deque;
  }

  auto itr = directory_map->find(fd);
  if (itr == directory_map->end()) {
    LeaveCriticalSection(&g_critical_section.critical_section_);
    return empty_deque;
  }

  std::deque<std::string> next_directory_entry = itr->second;
  if (erase) {
    directory_map->erase(itr);
  }
  LeaveCriticalSection(&g_critical_section.critical_section_);
  return next_directory_entry;
}

static void handle_db_replace(int fd,
                              std::deque<std::string> next_directory_entry) {
  EnterCriticalSection(&g_critical_section.critical_section_);
  if (directory_map == nullptr) {
    directory_map = new std::map<int, std::deque<std::string>>();
  }
  auto itr = directory_map->find(fd);
  if (itr == directory_map->end()) {
    directory_map->insert({fd, next_directory_entry});
  } else {
    directory_map->erase(itr);
    directory_map->insert({fd, next_directory_entry});
  }
  LeaveCriticalSection(&g_critical_section.critical_section_);
}

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

extern "C" {

DIR* opendir(const char* path) {
  using starboard::shared::win32::CStringToWString;
  using starboard::shared::win32::NormalizeWin32Path;

  if ((path == nullptr) || (path[0] == '\0')) {
    errno = ENOENT;
    return nullptr;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);

  if (!starboard::shared::win32::IsAbsolutePath(path_wstring)) {
    errno = EBADF;
    return nullptr;
  }

  HANDLE directory_handle =
      starboard::shared::win32::OpenFileOrDir(path, O_RDONLY);

  if (!starboard::shared::win32::IsValidHandle(directory_handle)) {
    errno = EBADF;
    return nullptr;
  }

  FILE_BASIC_INFO basic_info = {0};
  BOOL basic_info_success = GetFileInformationByHandleEx(
      directory_handle, FileBasicInfo, &basic_info, sizeof(FILE_BASIC_INFO));

  if (!basic_info_success ||
      !(basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    CloseHandle(directory_handle);
    errno = ENOTDIR;
    return nullptr;
  }

  DIR* dir = reinterpret_cast<DIR*>(calloc(1, sizeof(DIR)));
  dir->handle = directory_handle;
  dir->fd = handle_db_put(std::deque<std::string>());
  return dir;
}

int closedir(DIR* dir) {
  if (!dir) {
    errno = EBADF;
    return -1;
  }
  bool success = CloseHandle(dir->handle);
  handle_db_get(dir->fd, true);
  free(dir);
  if (!success) {
    errno = EBADF;
    return -1;
  }
  return 0;
}

int readdir_r(DIR* __restrict dir,
              struct dirent* __restrict dirent_buf,
              struct dirent** __restrict dirent) {
  if (!dir || !dirent_buf || !dirent) {
    if (dirent) {
      *dirent = nullptr;
    }
    return EBADF;
  }

  auto next_directory_entries = handle_db_get(dir->fd, false);
  if (next_directory_entries.empty()) {
    next_directory_entries = GetDirectoryEntries(dir->handle);
  }

  if (next_directory_entries.empty()) {
    *dirent = nullptr;
    return ENOENT;
  }

  if (starboard::strlcpy(dirent_buf->d_name,
                         next_directory_entries.rbegin()->c_str(),
                         kSbFileMaxName) >= kSbFileMaxName) {
    *dirent = nullptr;
    return ENOENT;
  }

  *dirent = dirent_buf;
  next_directory_entries.pop_back();
  handle_db_replace(dir->fd, next_directory_entries);

  return 0;
}

}  // extern "C"
