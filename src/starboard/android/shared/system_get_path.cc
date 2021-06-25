// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/directory.h"

using ::starboard::android::shared::g_app_assets_dir;
using ::starboard::android::shared::g_app_cache_dir;
using ::starboard::android::shared::g_app_lib_dir;

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  const int kPathSize = PATH_MAX;
  char path[kPathSize];
  path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory: {
      if (starboard::strlcat(path, g_app_assets_dir, kPathSize) >= kPathSize) {
        return false;
      }
      break;
    }

    case kSbSystemPathCacheDirectory: {
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path, "/cache", kPathSize) >= kPathSize) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathDebugOutputDirectory: {
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path, "/log", kPathSize) >= kPathSize) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathTempDirectory: {
      if (starboard::strlcpy(path, g_app_cache_dir, kPathSize) >= kPathSize) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathTestOutputDirectory: {
      return SbSystemGetPath(kSbSystemPathDebugOutputDirectory, out_path,
                             path_size);
    }

    // We return the library directory as the "executable" since:
    // a) Unlike the .so itself, it has a valid timestamp of the app install.
    // b) Its parent directory is still a directory within our app package.
    case kSbSystemPathExecutableFile: {
      if (starboard::strlcpy(path, g_app_lib_dir, kPathSize) >= kPathSize) {
        return false;
      }
      break;
    }

    default:
      SB_NOTIMPLEMENTED();
      return false;
  }

  int length = strlen(path);
  if (length < 1 || length > path_size) {
    return false;
  }

  starboard::strlcpy(out_path, path, path_size);
  return true;
}
