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

#include "cobalt/script/environment_settings.h"

#include "base/guid.h"

namespace cobalt {
namespace script {

EnvironmentSettings::EnvironmentSettings(
    const base::DebuggerHooks& debugger_hooks)
    : debugger_hooks_(debugger_hooks) {
  uuid_ = base::GenerateGUID();
}


// static
const base::NullDebuggerHooks EnvironmentSettings::null_debugger_hooks_;

}  // namespace script
}  // namespace cobalt
