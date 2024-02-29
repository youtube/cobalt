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

#include "v8/include/v8.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cobalt {

namespace script {
class EnvironmentSettings;
class GlobalEnvironment;
class ScriptValueFactory;
class Wrappable;
}  // namespace script

namespace web {

class Context;

Context* get_context(script::EnvironmentSettings* environment_settings);
v8::Isolate* get_isolate(script::EnvironmentSettings* environment_settings);
script::GlobalEnvironment* get_global_environment(
    script::EnvironmentSettings* environment_settings);
script::Wrappable* get_global_wrappable(
    script::EnvironmentSettings* environment_settings);
script::ScriptValueFactory* get_script_value_factory(
    script::EnvironmentSettings* environment_settings);
base::SequencedTaskRunner* get_task_runner(
    script::EnvironmentSettings* environment_settings);

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_ENVIRONMENT_SETTINGS_HELPER_H_
