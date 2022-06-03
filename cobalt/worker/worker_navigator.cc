// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/worker/worker_navigator.h"

#include <memory>
#include <vector>

#include "base/optional.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/navigator_ua_data.h"
#include "starboard/configuration_constants.h"

namespace cobalt {
namespace worker {

WorkerNavigator::WorkerNavigator(
    script::EnvironmentSettings* settings, const std::string& user_agent,
    web::UserAgentPlatformInfo* platform_info, const std::string& language,
    script::ScriptValueFactory* script_value_factory)
    : web::NavigatorBase(settings, user_agent, platform_info, language,
                         script_value_factory) {}

}  // namespace worker
}  // namespace cobalt
