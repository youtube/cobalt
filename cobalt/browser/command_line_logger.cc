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

#include "cobalt/browser/command_line_logger.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace cobalt {

std::string CommandLineSwitchesToString(const base::CommandLine& command_line) {
  std::vector<std::string> switch_strings;
  for (const auto& pair : command_line.GetSwitches()) {
    switch_strings.push_back(base::StringPrintf(
        "'%s': '%s'", pair.first.c_str(), pair.second.c_str()));
  }
  return base::JoinString(switch_strings, "\n");
}

}  // namespace cobalt
