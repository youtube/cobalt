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

#ifndef COBALT_BROWSER_CLIENT_HINT_HEADERS_H_
#define COBALT_BROWSER_CLIENT_HINT_HEADERS_H_

#include <string>
#include <vector>

#include "cobalt/browser/user_agent_platform_info.h"

namespace cobalt {
namespace browser {

// This function is deterministic and non-dependent on global variables and
// state.  It is dependent only on |platform_info|.
std::vector<std::string> GetClientHintHeaders(
    const UserAgentPlatformInfo& platform_info);

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CLIENT_HINT_HEADERS_H_
