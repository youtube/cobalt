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
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"

using starboard::IsAndroidAssetPath;
using starboard::OpenAndroidAsset;
using starboard::OpenAndroidAssetDir;

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {
int __real_stat(const char* path, struct stat* info);

// This needs to be exported to ensure shared_library targets include it.
int __wrap_stat(const char* path, struct stat* info) {
  // SbFileExists(path) implementation for Android
  if (!IsAndroidAssetPath(path)) {
    return __real_stat(path, info);  // Using system level stat call
  }

  int file = open(path, O_RDONLY, S_IRUSR | S_IWUSR);
  if (file >= 0) {
    int result = fstat(file, info);
    close(file);
    return result;
  }

  // Values from SbFileGetPathInfo
  if (IsAndroidAssetPath(path)) {
    AAssetDir* asset_dir = OpenAndroidAssetDir(path);
    if (asset_dir) {
      info->st_mode = S_IFDIR;
      info->st_ctime = 0;
      info->st_atime = 0;
      info->st_mtime = 0;
      info->st_size = 0;
      AAssetDir_close(asset_dir);
      return 0;
    }
  }

  return -1;
}

}  // extern "C"
