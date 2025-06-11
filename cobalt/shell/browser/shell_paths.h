// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_BROWSER_SHELL_PATHS_H_
#define COBALT_SHELL_BROWSER_SHELL_PATHS_H_

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

#endif  // COBALT_SHELL_BROWSER_SHELL_PATHS_H_
