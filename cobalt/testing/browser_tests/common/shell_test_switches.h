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

// Defines all the "cobalt_shell" command-line switches for browser tests.

#ifndef COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_TEST_SWITCHES_H_
#define COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_TEST_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

// Exposes the window.internals object to JavaScript for interactive development
// and debugging of web tests that rely on it.
extern const char kExposeInternalsForTesting[];

// Runs Cobalt Shell in web test mode, injecting test-only behaviour for
// blink web tests.
extern const char kRunWebTests[];

// Helper that returns true if kRunWebTests is present in the command line,
// meaning Cobalt Shell is running in web test mode.
bool IsRunWebTestsSwitchPresent();

}  // namespace switches

#endif  // COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_TEST_SWITCHES_H_
