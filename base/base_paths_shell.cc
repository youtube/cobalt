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

extern std::string *global_dir_source_root;
extern std::string *global_game_content_path;
extern std::string *global_tmp_path;

namespace base {

// this is where we can control the path for placement
// of a lot of file resources for lbshell.
bool PathProviderShell(int key, FilePath* result) {
  switch (key) {
    case base::DIR_EXE:
    case base::DIR_MODULE:
      *result = FilePath(*global_game_content_path);
      return true;

    case base::DIR_SOURCE_ROOT:
#if !defined(__LB_SHELL__FOR_RELEASE__)
      *result = FilePath(*global_dir_source_root);
      return true;
#else
      DLOG(ERROR) << "DIR_SOURCE_ROOT unsupported";
      return false;
#endif

    case base::DIR_CACHE:
      *result = FilePath(*global_tmp_path).Append("cache");
      return true;
  }

  return false;
}

}
