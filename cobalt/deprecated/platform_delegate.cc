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

#include "cobalt/deprecated/platform_delegate.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "lbshell/src/lb_globals.h"

namespace cobalt {
namespace deprecated {
namespace {

bool GetOrCreateDirectory(const char* directory) {
  return directory && (file_util::PathExists(FilePath(directory)) ||
                       file_util::CreateDirectory(FilePath(directory)));
}

bool PathProvider(int key, FilePath* result) {
  const global_values_t* global_values = GetGlobalsPtr();
  if (!global_values) {
    return false;
  }

  switch (key) {
    case paths::DIR_COBALT_DEBUG_OUT:
      if (GetOrCreateDirectory(global_values->logging_output_path)) {
        *result = FilePath(global_values->logging_output_path);
        return true;
      } else {
        return false;
      }

    case paths::DIR_COBALT_TEST_OUT:
      // TODO(***REMOVED***): Create a special directory for tests.
      if (GetOrCreateDirectory(global_values->logging_output_path)) {
        *result = FilePath(global_values->logging_output_path);
        return true;
      } else {
        return false;
      }

    case paths::DIR_COBALT_WEB_ROOT:
      *result = FilePath(global_values->game_content_path).Append("web");
      return true;

    default:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace

// static
void PlatformDelegate::Init() {
  PlatformInit();

  PlatformMediaInit();

  // Register a path provider for Cobalt-specific paths.
  PathService::RegisterProvider(&PathProvider, paths::PATH_COBALT_START,
                                paths::PATH_COBALT_END);
}

// static
void PlatformDelegate::Teardown() {
  PlatformMediaTeardown();

  PlatformTeardown();
}

}   // namespace deprecated
}   // namespace cobalt
