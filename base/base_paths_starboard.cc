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
    case base::FILE_EXE:
      SbSystemGetPath(kSbSystemPathExecutableFile, path,
                      SB_ARRAY_SIZE_INT(path));
      *result = FilePath(path);
      return true;

    case base::DIR_EXE:
    case base::DIR_MODULE:
      SbSystemGetPath(kSbSystemPathContentDirectory, path,
                      SB_ARRAY_SIZE_INT(path));
      *result = FilePath(path);
      return true;

    case base::DIR_SOURCE_ROOT:
      // TODO: When Cobalt's primary use-case ceases to be serving web-pages
      //       locally, we should add a check here to ensure that release builds
      //       do not try to access DIR_SOURCE_ROOT.  b/22359828
      SbSystemGetPath(kSbSystemPathSourceDirectory, path,
                      SB_ARRAY_SIZE_INT(path));
      *result = FilePath(path);
      return true;

    case base::DIR_CACHE:
      SbSystemGetPath(kSbSystemPathCacheDirectory, path,
                      SB_ARRAY_SIZE_INT(path));
      *result = FilePath(path);
      return true;

    case base::DIR_TEMP:
      SbSystemGetPath(kSbSystemPathTempDirectory, path,
                      SB_ARRAY_SIZE_INT(path));
      *result = FilePath(path);
      return true;

    case base::DIR_HOME:
      // TODO: Add a home directory to SbSystemPathId and get it from there.
      return PathProviderStarboard(base::DIR_CACHE, result);
  }

  NOTREACHED() << "key = " << key;
  return false;
}

}
