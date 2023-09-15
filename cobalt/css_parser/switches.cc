// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/css_parser/switches.h"

#include <map>

namespace cobalt {
namespace css_parser {
namespace switches {

#if !defined(COBALT_BUILD_TYPE_GOLD)
const char kOnCssError[] = "on_css_error";
const char kOnCssErrorHelp[] =
    "If set to \"crash\", crashes on CSS error even when recoverable.";

const char kOnCssWarning[] = "on_css_warning";
const char kOnCssWarningHelp[] = "If set to \"crash\", crashes on CSS warning.";
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

std::string HelpMessage() {
  std::string help_message;
  std::map<std::string, const char*> help_map {
#if !defined(COBALT_BUILD_TYPE_GOLD)
    {kOnCssError, kOnCssErrorHelp}, {kOnCssWarning, kOnCssWarningHelp},
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
  };

  for (const auto& switch_message : help_map) {
    help_message.append("  --")
        .append(switch_message.first)
        .append("\n")
        .append("  ")
        .append(switch_message.second)
        .append("\n\n");
  }
  return help_message;
}

}  // namespace switches
}  // namespace css_parser
}  // namespace cobalt
