// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_COBALT_PATHS_H_
#define COBALT_BASE_COBALT_PATHS_H_

namespace cobalt {
namespace paths {

// The following keys are used to retrieve Cobalt-specific paths using
// base::PathService::Get().
//
// Example:
// --------
//   base::FilePath log_directory;
//   base::PathService::Get(paths::DIR_COBALT_DEBUG_OUT, &log_directory);
//

enum CobaltPathKeys {
  // Unique key which should not collide with other path provider keys.
  PATH_COBALT_START = 1000,

  // Directory where all Cobalt debug output (such as logs) should be stored.
  DIR_COBALT_DEBUG_OUT,

  // Directory where tests can write data such as expected results.
  DIR_COBALT_TEST_OUT,

  // Root directory for local web files (those fetched from file://).
  DIR_COBALT_WEB_ROOT,

  // End of Cobalt keys.
  PATH_COBALT_END,
};

}  // namespace paths
}  // namespace cobalt

#endif  // COBALT_BASE_COBALT_PATHS_H_
