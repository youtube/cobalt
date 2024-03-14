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

#include <sys/stat.h>
// Undef alias to `stat` and pull in the system-level header so we can use it
#undef stat
#include <../../usr/include/sys/stat.h>

#include <android/asset_manager.h>

#include "starboard/android/shared/directory_internal.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/directory.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

// Reverse implementation of TimeTToWindowsUsec and PosixTimeToWindowsTime for
// backwards compatibility TimeTToWindowsUsec converts to microseconds
// (*1000000) and then calls PosixTimeToWindowsTime PosixTimeToWindows time adds
// number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970 (UTC)
static SB_C_FORCE_INLINE time_t WindowsUsecToTimeTAndroid(int64_t time) {
  int64_t posix_time = time - 11644473600000000ULL;
  posix_time = posix_time / 1000000;
  return posix_time;
}

static void MapSbFileInfoToStat(SbFileInfo* file_info, struct stat* stat_info) {
  stat_info->st_mode = 0;
  if (file_info->is_directory) {
    stat_info->st_mode = S_IFDIR;
  } else if (file_info->is_symbolic_link) {
    stat_info->st_mode = S_IFLNK;
  }

  stat_info->st_ctime = WindowsUsecToTimeTAndroid(file_info->creation_time);
  stat_info->st_atime = WindowsUsecToTimeTAndroid(file_info->last_accessed);
  stat_info->st_mtime = WindowsUsecToTimeTAndroid(file_info->last_modified);
  stat_info->st_size = file_info->size;
}

// This needs to be exported to ensure shared_library targets include it.
int sb_stat(const char* path, struct stat* info) {
  // SbFileExists(path) implementation for Android
  if (!IsAndroidAssetPath(path)) {
    return stat(path, info);  // Using system level stat call
  }

  SbFile file = SbFileOpen(path, kSbFileRead | kSbFileOpenOnly, NULL, NULL);
  SbFileInfo out_info;
  if (file) {
    bool result = SbFileGetInfo(file, &out_info);
    MapSbFileInfoToStat(&out_info, info);
    SbFileClose(file);
    return 0;
  }

  // Values from SbFileGetPathInfo
  SbDirectory directory = SbDirectoryOpen(path, NULL);
  if (directory && directory->asset_dir) {
    info->st_mode = S_IFDIR;
    info->st_ctime = 0;
    info->st_atime = 0;
    info->st_mtime = 0;
    info->st_size = 0;
    SbDirectoryClose(directory);
    return 0;
  }

  return -1;
}

}  // extern "C"
