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

#ifndef COBALT_STARBOARD_WASM_CORE_MODULE_H_
#define COBALT_STARBOARD_WASM_CORE_MODULE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace cobalt {
namespace starboard_wasm {

class EventQueue;

int64_t MonotonicNowUs();

// What a module gets from the execution context it serves.
struct ModuleContext {
  // The JS thread that calls the module's imports.
  scoped_refptr<base::SequencedTaskRunner> owner;
  // Events for the guest; outlives the module.
  raw_ref<EventQueue> events;
};

// The implementation of one area of the Starboard API (system, player, ...)
// exposed to a Wasm guest. Its public methods mirror the area's Web IDL
// interface, which a Wasm module imports directly. A ProviderCore creates one
// instance of each module per execution context and destroys them when the
// context goes away.
class Module {
 public:
  virtual ~Module() = default;

  // Stable identifier, e.g. "player". Subclasses also define it as kName.
  virtual const char* name() const = 0;
};

using ModuleFactory = std::unique_ptr<Module> (*)(const ModuleContext& context);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_MODULE_H_
