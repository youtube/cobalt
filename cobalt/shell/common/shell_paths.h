// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_COMMON_SHELL_PATHS_H_
#define COBALT_SHELL_COMMON_SHELL_PATHS_H_

#include "build/build_config.h"

namespace content {

enum {
  SHELL_PATH_START = 12000,

  // Directory where user data can be written.
  SHELL_DIR_USER_DATA = SHELL_PATH_START,

  // TODO(jam): move from content/common since it's test only.
  // DIR_TEST_DATA,

  SHELL_PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterShellPathProvider();

}  // namespace content

#endif  // COBALT_SHELL_COMMON_SHELL_PATHS_H_
