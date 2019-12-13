// Copyright 2018 Google Inc. All Rights Reserved.
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
#include "base/files/file_path.h"
#include "base/logging.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace base {

// This is where we can control the path for placement of a lot of file
// resources for cobalt.
bool PathProviderStarboard(int key, FilePath *result) {
  std::vector<char> path(kSbFileMaxPath, 0);
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {
      bool success = SbSystemGetPath(kSbSystemPathExecutableFile, path.data(),
                                     path.size());
      DCHECK(success);
      if (success) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(ERROR) << "FILE_EXE not defined.";
      return false;
    }

    case base::DIR_EXE:
    case base::DIR_MODULE:
    case base::DIR_ASSETS: {
      bool success = SbSystemGetPath(kSbSystemPathContentDirectory, path.data(),
                                     path.size());
      DCHECK(success);
      if (success) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(ERROR) << "DIR_EXE/DIR_MODULE not defined.";
      return false;
    }

#if defined(ENABLE_TEST_DATA)
    case base::DIR_TEST_DATA: {
      bool success = SbSystemGetPath(kSbSystemPathContentDirectory, path.data(),
                                     path.size());
      DCHECK(success);
      if (success) {
        // Append "test" to match the output of copy_test_data.gypi
        *result = FilePath(path.data()).Append(FILE_PATH_LITERAL("test"));
        return true;
      }
      DLOG(ERROR) << "DIR_TEST_DATA not defined.";
      return false;
    }
#endif  // ENABLE_TEST_DATA

    case base::DIR_CACHE: {
      bool success = SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                                     path.size());
      if (success) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(INFO) << "DIR_CACHE not defined.";
      return false;
    }

    case base::DIR_TEMP: {
      bool success =
          SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), path.size());
      DCHECK(success);
      if (success) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(ERROR) << "DIR_TEMP not defined.";
      return false;
    }

    case base::DIR_HOME:
      // TODO: Add a home directory to SbSystemPathId and get it from there.
      return PathProviderStarboard(base::DIR_CACHE, result);

    case base::DIR_SYSTEM_FONTS:
      if (SbSystemGetPath(kSbSystemPathFontDirectory, path.data(),
                          path.size())) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(INFO) << "DIR_SYSTEM_FONTS not defined.";
      return false;

    case base::DIR_SYSTEM_FONTS_CONFIGURATION:
      if (SbSystemGetPath(kSbSystemPathFontConfigurationDirectory, path.data(),
                          path.size())) {
        *result = FilePath(path.data());
        return true;
      }
      DLOG(INFO) << "DIR_SYSTEM_FONTS_CONFIGURATION not defined.";
      return false;
  }

  return false;
}

}
