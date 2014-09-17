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
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "lbshell/src/lb_globals.h"

namespace cobalt {
namespace deprecated {
namespace {

bool PathProvider(int key, FilePath* result) {
  const global_values_t* global_values = GetGlobalsPtr();
  if (!global_values) {
    return false;
  }

  switch (key) {
    case paths::DIR_COBALT_LOGS:
      if (global_values->logging_output_path) {
        *result = FilePath(global_values->logging_output_path);
        return true;
      }
      return false;

    case paths::DIR_COBALT_SCREENSHOTS:
      if (global_values->screenshot_output_path) {
        *result = FilePath(global_values->screenshot_output_path);
        return true;
      }
      return false;

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
