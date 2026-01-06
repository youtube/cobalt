// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_COMMAND_LINE_DEBUGGING_TEST_SUPPORT_H_
#define COBALT_BROWSER_COMMAND_LINE_DEBUGGING_TEST_SUPPORT_H_

#include <string>

#include "base/command_line.h"

namespace cobalt {

// Formats command line switches into a human-readable string for debugging.
std::string FormatCommandLineSwitchesForDebugging(
    const base::CommandLine& command_line);

}  // namespace cobalt

#endif  // COBALT_BROWSER_COMMAND_LINE_DEBUGGING_TEST_SUPPORT_H_
