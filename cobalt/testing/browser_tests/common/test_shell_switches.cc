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

#include "cobalt/testing/browser_tests/common/test_shell_switches.h"

#include "base/command_line.h"

// TODO(b/452256746): rename namespace test_switches to switches when all
// references to switches are migrated.
namespace test_switches {

const char kExposeInternalsForTesting[] = "expose-internals-for-testing";

const char kRunWebTests[] = "run-web-tests";

bool IsRunWebTestsSwitchPresent() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      test_switches::kRunWebTests);
}

}  // namespace test_switches
