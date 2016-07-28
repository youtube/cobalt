/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/deprecated/platform_delegate.h"

namespace base {

// this is where we can control the path for placement
// of a lot of file resources for lbshell.
bool PathProviderShell(int key, FilePath* result) {
  cobalt::deprecated::PlatformDelegate* plat =
      cobalt::deprecated::PlatformDelegate::Get();
  switch (key) {
    case base::DIR_EXE:
    case base::DIR_MODULE:
      DCHECK(!plat->game_content_path().empty());
      *result = FilePath(plat->game_content_path());
      return true;

#if defined(ENABLE_DIR_SOURCE_ROOT_ACCESS)
    case base::DIR_SOURCE_ROOT:
      if (plat->dir_source_root().length() > 0) {
        *result = FilePath(plat->dir_source_root());
        return true;
      } else {
        DLOG(INFO) << "DIR_SOURCE_ROOT not defined - skipping.";
        return false;
      }
#endif  // ENABLE_DIR_SOURCE_ROOT_ACCESS

    case base::DIR_TEMP:
      DCHECK(!plat->temp_path().empty());
      *result = FilePath(plat->temp_path());
      return true;

    case base::DIR_CACHE:
      DCHECK(!plat->temp_path().empty());
      *result = FilePath(plat->temp_path()).Append("cache");
      return true;
    case base::DIR_HOME: {
#if defined(COBALT_LINUX)
      const char* home_dir = getenv("HOME");
      if (home_dir && home_dir[0]) {
        *result = FilePath(home_dir);
        return true;
      } else {
        return PathProviderShell(base::DIR_TEMP, result);
      }
#else
      return PathProviderShell(base::DIR_EXE, result);
#endif  // defined(COBALT_LINUX)
    }
  }

  return false;
}

}  // namespace base

