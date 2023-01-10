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

#ifndef COBALT_WEB_ENVIRONMENT_SETTINGS_HELPER_H_
#define COBALT_WEB_ENVIRONMENT_SETTINGS_HELPER_H_

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace web {

Context* get_context(script::EnvironmentSettings* environment_settings);
v8::Isolate* get_isolate(script::EnvironmentSettings* environment_settings);
script::GlobalEnvironment* get_global_environment(
    script::EnvironmentSettings* environment_settings);
script::Wrappable* get_global_wrappable(
    script::EnvironmentSettings* environment_settings);
script::ScriptValueFactory* get_script_value_factory(
    script::EnvironmentSettings* environment_settings);
base::MessageLoop* get_message_loop(
    script::EnvironmentSettings* environment_settings);

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_ENVIRONMENT_SETTINGS_HELPER_H_
