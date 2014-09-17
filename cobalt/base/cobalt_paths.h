/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef BASE_COBALT_PATHS_H_
#define BASE_COBALT_PATHS_H_

namespace cobalt {
namespace paths {

// The following keys are used to retreive Cobalt-specific paths using
// PathService::Get().
//
// Example:
// --------
//   FilePath log_directory;
//   PathService::Get(paths::DIR_COBALT_LOGS, &log_directory);
//

enum CobaltPathKeys {
  // Unique key which should not collide with other path provider keys.
  PATH_COBALT_START = 1000,

  // Directory where Cobalt logs should be stored.
  DIR_COBALT_LOGS,
  // Directory where Cobalt screenshots should be stored.
  DIR_COBALT_SCREENSHOTS,

  // End of Cobalt keys.
  PATH_COBALT_END,
};

}  // namespace paths
}  // namespace cobalt

#endif  // BASE_COBALT_PATHS_H_
