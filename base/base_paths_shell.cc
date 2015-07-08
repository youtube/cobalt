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
#include "lb_globals.h"

namespace base {

// this is where we can control the path for placement
// of a lot of file resources for lbshell.
bool PathProviderShell(int key, FilePath* result) {
  const global_values_t* global_values = GetGlobalsPtr();
  switch (key) {
    case base::DIR_EXE:
    case base::DIR_MODULE:
      *result = FilePath(global_values->game_content_path);
      return true;

    case base::DIR_SOURCE_ROOT:
      // TODO(aabtop): When Cobalt's primary use-case ceases to be serving
      //               web-pages locally, we should add a check here to ensure
      //               that release builds do not try to access DIR_SOURCE_ROOT.
      //               b/22359828
      *result = FilePath(global_values->dir_source_root);
      return true;

    case base::DIR_CACHE:
      *result = FilePath(global_values->tmp_path).Append("cache");
      return true;
  }

  return false;
}

}
