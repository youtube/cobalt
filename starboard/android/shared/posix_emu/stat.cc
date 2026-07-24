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

#include <android/asset_manager.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "starboard/android/shared/file_internal.h"

using starboard::IsAndroidAssetPath;
using starboard::ListAndroidAssetDir;
using starboard::OpenAndroidAsset;

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

int __real_fstatat(int dirfd, const char* path, struct stat* info, int flags);
int __wrap_fstatat(int dirfd, const char* path, struct stat* info, int flags);

int __wrap_stat(const char* path, struct stat* info) {
  return __wrap_fstatat(AT_FDCWD, path, info, 0);
}

int __wrap_fstatat(int dirfd, const char* path, struct stat* info, int flags) {
  if (!IsAndroidAssetPath(path)) {
    return __real_fstatat(dirfd, path, info, flags);
  }

  if (info == NULL) {
    errno = EFAULT;
    return -1;
  }

  // Asset paths are absolute (/cobalt/assets/...), so `dirfd` is irrelevant.
  if (AAsset* asset = OpenAndroidAsset(path)) {
    // Assets are immutable APK content, so report them read-only.
    info->st_mode = S_IFREG | S_IRUSR;  // Read-only regular file.
    info->st_ctime = 0;
    info->st_atime = 0;
    info->st_mtime = 0;
    info->st_size = AAsset_getLength(asset);
    AAsset_close(asset);
    return 0;
  }

  if (!ListAndroidAssetDir(path).empty()) {
    info->st_mode = S_IFDIR | S_IRUSR | S_IXUSR;  // Read-only, traversable dir.
    info->st_ctime = 0;
    info->st_atime = 0;
    info->st_mtime = 0;
    info->st_size = 0;
    return 0;
  }

  errno = ENOENT;
  return -1;
}

}  // extern "C"
