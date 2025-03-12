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

#include <string>

#include "base/command_line.h"

#ifndef COBALT_BROWSER_SWITCHES_H_
#define COBALT_BROWSER_SWITCHES_H_

namespace cobalt {

constexpr char kDefaultURL[] = "https://www.youtube.com/tv";

namespace switches {

// Allow the user to override the default URL via a command line parameter.
std::string GetInitialURL(const base::CommandLine& command_line);

constexpr char kInitialURL[] = "url";

}  // namespace switches
}  // namespace cobalt

#endif  // COBALT_BROWSER_SWITCHES_H_
