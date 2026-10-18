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

#include "cobalt/starboard_wasm/core/provider_core.h"

#include <string.h>

#include <chrono>
#include <utility>

#include "cobalt/starboard_wasm/core/event_queue.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"
#include "v8/include/v8-wasm.h"

namespace cobalt {
namespace starboard_wasm {

int64_t MonotonicNowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

ProviderCore::ProviderCore(scoped_refptr<base::SequencedTaskRunner> owner,
                           base::RepeatingClosure on_wake,
                           const std::vector<ModuleFactory>& modules)
    : events_(std::make_unique<EventQueue>(owner, std::move(on_wake))) {
  const ModuleContext context{owner, raw_ref(*events_)};
  for (ModuleFactory factory : modules) {
    modules_.push_back(factory(context));
  }
}

ProviderCore::~ProviderCore() {
  Shutdown();
}

bool ProviderCore::BindMemory(v8::Isolate* isolate,
                              v8::Local<v8::Value> memory,
                              std::string* error) {
  if (!events_) {
    *error = "provider is shut down";
    return false;
  }
  if (memory.IsEmpty() || !memory->IsWasmMemoryObject()) {
    *error = "bindMemory() expects a WebAssembly.Memory";
    return false;
  }
  memory_.Reset(isolate, memory.As<v8::WasmMemoryObject>());
  return true;
}

Module* ProviderCore::GetModule(const char* name) const {
  for (const std::unique_ptr<Module>& module : modules_) {
    if (strcmp(module->name(), name) == 0) {
      return module.get();
    }
  }
  return nullptr;
}

void ProviderCore::Shutdown() {
  // Modules may still queue events while stopping their threads.
  modules_.clear();
  events_.reset();
  memory_.Reset();
}

// static
double ProviderCore::NowUs() {
  return static_cast<double>(MonotonicNowUs());
}

}  // namespace starboard_wasm
}  // namespace cobalt
