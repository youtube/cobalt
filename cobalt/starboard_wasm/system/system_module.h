// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STARBOARD_WASM_SYSTEM_SYSTEM_MODULE_H_
#define COBALT_STARBOARD_WASM_SYSTEM_SYSTEM_MODULE_H_

#include <memory>

#include "cobalt/starboard_wasm/core/module.h"

namespace cobalt {
namespace starboard_wasm {

// Creates the `sb_sys` imports: now_us, poll_events and log.
std::unique_ptr<Module> CreateSystemModule(const ModuleContext& context);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_SYSTEM_SYSTEM_MODULE_H_
