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

#include "cobalt/browser/command_line_debugging_helper.h"

#include <vector>

#include "base/strings/string_util.h"

namespace cobalt {

std::string FormatCommandLineSwitchesForDebugging(
    const base::CommandLine& command_line) {
  std::vector<std::string> formatted_switches;
  for (const auto& pair : command_line.GetSwitches()) {
    const std::string& key = pair.first;
    const base::CommandLine::StringType& value = pair.second;
    formatted_switches.push_back("'" + key + "': '" + value + "'");
  }
  return base::JoinString(formatted_switches, "\n");
}

}  // namespace cobalt
