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

#include "cobalt/worker/worker_settings.h"

#include "cobalt/base/debugger_hooks.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {
WorkerSettings::WorkerSettings() : web::EnvironmentSettings() {}

WorkerSettings::WorkerSettings(web::MessagePort* message_port)
    : web::EnvironmentSettings(), message_port_(message_port) {}
}  // namespace worker
}  // namespace cobalt
