// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/path_provider.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "cobalt/base/cobalt_paths.h"
#include "starboard/system.h"

namespace {
FilePath GetOrCreateDirectory(SbSystemPathId path_id) {
  scoped_array<char> path(new char[SB_FILE_MAX_PATH]);
  path[0] = '\0';
  if (SbSystemGetPath(path_id, path.get(), SB_FILE_MAX_PATH)) {
    FilePath directory(path.get());
    if (file_util::PathExists(directory) ||
        file_util::CreateDirectory(directory)) {
      return directory;
    }
  }
  return FilePath();
}
}  // namespace

namespace cobalt {

bool PathProvider(int key, FilePath* result) {
  switch (key) {
    case paths::DIR_COBALT_DEBUG_OUT: {
      FilePath directory =
          GetOrCreateDirectory(kSbSystemPathDebugOutputDirectory);
      if (!directory.empty()) {
        *result = directory;
        return true;
      } else {
        DLOG(ERROR) << "Unable to get or create paths::DIR_COBALT_DEBUG_OUT";
        return false;
      }
    }

    case paths::DIR_COBALT_TEST_OUT:
      // TODO: Create a special directory for tests.
      {
        FilePath directory =
            GetOrCreateDirectory(kSbSystemPathDebugOutputDirectory);
        if (!directory.empty()) {
          *result = directory;
          return true;
        } else {
          DLOG(ERROR) << "Unable to get or create paths::DIR_COBALT_TEST_OUT";
          return false;
        }
      }

    case paths::DIR_COBALT_WEB_ROOT: {
      FilePath directory = GetOrCreateDirectory(kSbSystemPathContentDirectory);
      if (!directory.empty()) {
        *result = directory.Append("web");
        return true;
      } else {
        DLOG(ERROR) << "Unable to get or create paths::DIR_COBALT_WEB_ROOT";
        return false;
      }
    }
      return true;

    default:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace cobalt
