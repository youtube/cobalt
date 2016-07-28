// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "starboard/system.h"

namespace base {

// This is where we can control the path for placement of a lot of file
// resources for cobalt.
bool PathProviderStarboard(int key, FilePath *result) {
  char path[SB_FILE_MAX_PATH];
  switch (key) {
    case base::FILE_EXE: {
      bool success = SbSystemGetPath(kSbSystemPathExecutableFile, path,
                      SB_ARRAY_SIZE_INT(path));
      DCHECK(success);
      if (success) {
        *result = FilePath(path);
        return true;
      }
      DLOG(ERROR) << "FILE_EXE not defined.";
      return false;
    }

    case base::DIR_EXE:
    case base::DIR_MODULE: {
      bool success = SbSystemGetPath(kSbSystemPathContentDirectory, path,
                                     SB_ARRAY_SIZE_INT(path));
      DCHECK(success);
      if (success) {
        *result = FilePath(path);
        return true;
      }
      DLOG(ERROR) << "DIR_EXE/DIR_MODULE not defined.";
      return false;
    }

#if defined(ENABLE_DIR_SOURCE_ROOT_ACCESS)
    case base::DIR_SOURCE_ROOT: {
      bool success = SbSystemGetPath(kSbSystemPathSourceDirectory, path,
                                     SB_ARRAY_SIZE_INT(path));
      DCHECK(success);
      if (success) {
        *result = FilePath(path);
        return true;
      }
      DLOG(ERROR) << "DIR_SOURCE_ROOT not defined.";
      return false;
    }
#endif  // ENABLE_DIR_SOURCE_ROOT_ACCESS

    case base::DIR_CACHE: {
      bool success = SbSystemGetPath(kSbSystemPathCacheDirectory, path,
                                     SB_ARRAY_SIZE_INT(path));
      if (success) {
        *result = FilePath(path);
        return true;
      }
      DLOG(INFO) << "DIR_CACHE not defined.";
      return false;
    }

    case base::DIR_TEMP: {
      bool success = SbSystemGetPath(kSbSystemPathTempDirectory, path,
                                     SB_ARRAY_SIZE_INT(path));
      DCHECK(success);
      if (success) {
        *result = FilePath(path);
        return true;
      }
      DLOG(ERROR) << "DIR_TEMP not defined.";
      return false;
    }

    case base::DIR_HOME:
      // TODO: Add a home directory to SbSystemPathId and get it from there.
      return PathProviderStarboard(base::DIR_CACHE, result);
  }

  NOTREACHED() << "key = " << key;
  return false;
}

}
