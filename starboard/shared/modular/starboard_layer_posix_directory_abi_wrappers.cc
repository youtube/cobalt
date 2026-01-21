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

#include "starboard/shared/modular/starboard_layer_posix_directory_abi_wrappers.h"

#include <string.h>

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

int __abi_wrap_readdir_r(musl_dir* dirp,
                         struct musl_dirent* musl_entry,
                         struct musl_dirent** musl_result) {
  // readdir_r segfaults if any of those parameters are missing.
  if (!dirp || !dirp->dir || !musl_entry || !musl_result) {
    errno = EBADF;
    return -1;
  }

  struct dirent entry = {0};  // The type from platform toolchain.
  struct dirent* result = nullptr;
  int retval = readdir_r(dirp->dir, &entry, &result);
  if (retval != 0) {
    return retval;
  }
#if !SB_HAS_QUIRK(INCOMPLETE_DIRENT_STRUCTURE)
  musl_entry->d_ino = entry.d_ino;
  musl_entry->d_off = entry.d_off;
#endif
  musl_entry->d_reclen = entry.d_reclen;
  musl_entry->d_type = entry.d_type;

  memset(musl_entry->d_name, 0, sizeof(musl_entry->d_name));
  constexpr auto minlen =
      std::min(sizeof(musl_entry->d_name), sizeof(entry.d_name));
  memcpy(musl_entry->d_name, entry.d_name, minlen);
  if (result == nullptr) {
    *musl_result = nullptr;
  } else {
    *musl_result = musl_entry;
  }
  return 0;
}

struct musl_dir* __abi_wrap_opendir(const char* name) {
  if (!name) {
    errno = ENOENT;
    return nullptr;
  }

  DIR* directory = opendir(name);

  if (!directory) {
    return nullptr;
  }

  musl_dir* musl_directory = (musl_dir*)calloc(1, sizeof(musl_dir));
  if (!musl_directory) {
    errno = ENOMEM;
    return nullptr;
  }
  musl_directory->dir = directory;

  musl_dirent* musl_dir_entry = (musl_dirent*)calloc(1, sizeof(musl_dirent));
  if (!musl_dir_entry) {
    errno = ENOMEM;
    free(musl_directory);
    return nullptr;
  }
  musl_directory->musl_dir_entry = musl_dir_entry;

  return musl_directory;
}

struct musl_dir* __abi_wrap_fdopendir(int fd) {
  DIR* directory = fdopendir(fd);

  if (!directory) {
    return nullptr;
  }

  musl_dir* musl_directory = (musl_dir*)calloc(1, sizeof(musl_dir));
  if (!musl_directory) {
    errno = ENOMEM;
    return nullptr;
  }
  musl_directory->dir = directory;

  musl_dirent* musl_dir_entry = (musl_dirent*)calloc(1, sizeof(musl_dirent));
  if (!musl_dir_entry) {
    errno = ENOMEM;
    free(musl_directory);
    return nullptr;
  }
  musl_directory->musl_dir_entry = musl_dir_entry;

  return musl_directory;
}

int __abi_wrap_closedir(musl_dir* musl_directory) {
  if (!musl_directory) {
    errno = EBADF;
    return -1;
  }

  DIR* directory = musl_directory->dir;
  free(musl_directory->musl_dir_entry);
  free(musl_directory);

  return closedir(directory);
}

struct musl_dirent* __abi_wrap_readdir(musl_dir* dirp) {
  // readdir fails if any of those parameters are missing.
  if (!dirp || !dirp->dir || !dirp->musl_dir_entry) {
    errno = EBADF;
    return nullptr;
  }

  struct dirent* result_platform = readdir(dirp->dir);
  if (!result_platform) {
    return nullptr;
  }

  memset(dirp->musl_dir_entry, 0, sizeof(musl_dirent));
#if !SB_HAS_QUIRK(INCOMPLETE_DIRENT_STRUCTURE)
  dirp->musl_dir_entry->d_ino = result_platform->d_ino;
  dirp->musl_dir_entry->d_off = result_platform->d_off;
#endif
  dirp->musl_dir_entry->d_reclen = result_platform->d_reclen;
  dirp->musl_dir_entry->d_type = result_platform->d_type;

  if (starboard::strlcpy(dirp->musl_dir_entry->d_name, result_platform->d_name,
                         sizeof(dirp->musl_dir_entry->d_name)) >=
      sizeof(dirp->musl_dir_entry->d_name)) {
    SB_LOG(WARNING) << "Truncated d_name in readdir wrapper."
                    << " src_size=" << sizeof(result_platform->d_name)
                    << " dst_size=" << sizeof(dirp->musl_dir_entry->d_name);
  }

  return dirp->musl_dir_entry;
}
