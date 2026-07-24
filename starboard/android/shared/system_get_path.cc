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

// clang-format off
#include "starboard/system.h"
// clang-format on

#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

#include "build/build_config.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/elf_loader/evergreen_config.h"
#include "third_party/jni_zero/jni_zero.h"

using ::starboard::g_app_assets_dir;
using ::starboard::g_app_cache_dir;
using ::starboard::g_app_files_dir;
using ::starboard::g_app_lib_dir;

#if BUILDFLAG(IS_STARBOARD)
namespace {

// Returns the base content path, which is the Evergreen content path when one
// is configured, or the app's asset directory otherwise.
const char* GetContentPath() {
  const elf_loader::EvergreenConfig* evergreen_config =
      elf_loader::EvergreenConfig::GetInstance();
  if (evergreen_config && !evergreen_config->content_path_.empty()) {
    return evergreen_config->content_path_.c_str();
  }
  return g_app_assets_dir;
}

}  // namespace
#endif

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

    case kSbSystemPathFontConfigurationDirectory:
    case kSbSystemPathFontDirectory:
#if BUILDFLAG(IS_STARBOARD)
      if (starboard::strlcpy(path, GetContentPath(), kPathSize) >= kPathSize) {
        return false;
      }
      if (starboard::strlcat(path, "/fonts", kPathSize) >= kPathSize) {
        return false;
      }
      break;
#else
      SB_NOTIMPLEMENTED();
      return false;
#endif

    case kSbSystemPathStorageDirectory: {
      if (starboard::strlcpy(path, g_app_files_dir, kPathSize) >= kPathSize) {
        return false;
      }
      if (starboard::strlcat(path, "/storage", kPathSize) >= kPathSize) {
        return false;
      }
      mkdir(path, 0700);
      break;
    }
    case kSbSystemPathFilesDirectory: {
      if (starboard::strlcpy(path, g_app_files_dir, kPathSize) >= kPathSize) {
        return false;
      }
      if (starboard::strlcat(path, "/files", kPathSize) >= kPathSize) {
        return false;
      }
      mkdir(path, 0700);
      break;
    }
    case kSbSystemPathCacheDirectory: {
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path, "/cache", kPathSize) >= kPathSize) {
        return false;
      }

      mkdir(path, 0700);
      break;
    }

    case kSbSystemPathDebugOutputDirectory: {
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path, "/log", kPathSize) >= kPathSize) {
        return false;
      }

      mkdir(path, 0700);
      break;
    }

    case kSbSystemPathTempDirectory: {
      if (starboard::strlcpy(path, g_app_cache_dir, kPathSize) >= kPathSize) {
        return false;
      }

      mkdir(path, 0700);
      break;
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
