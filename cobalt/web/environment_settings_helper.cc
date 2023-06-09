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

#include "cobalt/web/environment_settings_helper.h"

#include "base/message_loop/message_loop.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"

namespace cobalt {
namespace web {

Context* get_context(script::EnvironmentSettings* environment_settings) {
  if (!environment_settings) {
    return nullptr;
  }
  auto* settings =
      base::polymorphic_downcast<EnvironmentSettings*>(environment_settings);
  return settings->context();
}

script::GlobalEnvironment* get_global_environment(
    script::EnvironmentSettings* environment_settings) {
  auto* context = get_context(environment_settings);
  if (!context) {
    return nullptr;
  }
  return context->global_environment();
}

v8::Isolate* get_isolate(script::EnvironmentSettings* environment_settings) {
  auto* global_environment = get_global_environment(environment_settings);
  if (!global_environment) {
    return nullptr;
  }
  return global_environment->isolate();
}

script::Wrappable* get_global_wrappable(
    script::EnvironmentSettings* environment_settings) {
  auto* global_environment = get_global_environment(environment_settings);
  if (!global_environment) {
    return nullptr;
  }
  return global_environment->global_wrappable();
}

script::ScriptValueFactory* get_script_value_factory(
    script::EnvironmentSettings* environment_settings) {
  auto* global_environment = get_global_environment(environment_settings);
  if (!global_environment) {
    return nullptr;
  }
  return global_environment->script_value_factory();
}

base::MessageLoop* get_message_loop(
    script::EnvironmentSettings* environment_settings) {
  auto* context = get_context(environment_settings);
  if (!context) {
    return nullptr;
  }
  return context->message_loop();
}

}  // namespace web
}  // namespace cobalt
