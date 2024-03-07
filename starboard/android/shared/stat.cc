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
#include <sys/stat.h>

#include "starboard/directory.h"

#include "starboard/android/shared/file_internal.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;
extern "C" {

void mapSbFileInfoToStat(SbFileInfo* file_info, struct stat* stat_info) {
  stat_info->st_mode = 0;
  if (file_info.is_directory) {
    stat_info->st_mode = S_IFDIR;
  } else if (file_info.is_symbolic_link) {
    stat_info->st_mode = S_IFLNK;
  }

  stat_info->st_ctime = WindowsUsecToTimeT(file_info.creation_time);
  stat_info->st_atime = WindowsUsecToTimeT(file_info.last_accessed);
  stat_info->st_mtime = WindowsUsecToTimeT(file_info.last_modified);
  stat_info->st_size = file_info.size;
}

int __real_stat(const char* path, struct stat info);

// This needs to be exported to ensure shared_library targets include it.
SB_EXPORT_PLATFORM int __wrap_stat(const char* path, struct stat info) {
  // SbFileExists(path) implementation for Android
  if (!IsAndroidAssetPath(path)) {
    return __real_stat(path, &info);
  }

  SbFileInfo out_info;
  if (file) {
    bool result = SbFileGetInfo(file, out_info);
    mapSbFileInfoToStat(&out_info, &info);
    SbFileClose(file);
    return 0;
  }

  SbDirectory directory = SbDirectoryOpen(path, NULL);
  if (directory && directory->asset_dir) {
    info->st_mode = S_IFDIR;
    info->st_ctime = WindowsUsecToTimeT(file_info.creation_time);
    info->st_atime = WindowsUsecToTimeT(file_info.last_accessed);
    info->st_mtime = WindowsUsecToTimeT(file_info.last_modified);
    info->st_size = file_info.size;
    SbDirectoryClose(directory);
    return 0;
  }

  return -1;
}
}
