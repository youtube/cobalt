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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_system.h"

#include "cobalt/starboard_wasm/core/provider_core.h"
#include "cobalt/starboard_wasm/system/system_module.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports.h"

namespace blink {

using cobalt::starboard_wasm::GuestMemory;
using cobalt::starboard_wasm::ProviderCore;
using cobalt::starboard_wasm::SystemModule;

H5vccWasmSystem::H5vccWasmSystem(H5vccWasmImports& imports)
    : imports_(&imports) {}

double H5vccWasmSystem::nowUs() {
  return ProviderCore::NowUs();
}

uint32_t H5vccWasmSystem::pollEvents(v8::Isolate* isolate,
                                     uint32_t events,
                                     uint32_t count) {
  ProviderCore* core = imports_->core();
  SystemModule* system = core ? core->GetModule<SystemModule>() : nullptr;
  if (!system) {
    return 0;
  }
  GuestMemory memory = core->Memory(isolate);
  return system->PollEvents(memory, events, count);
}

void H5vccWasmSystem::log(v8::Isolate* isolate,
                          uint32_t level,
                          uint32_t message,
                          uint32_t message_length) {
  ProviderCore* core = imports_->core();
  SystemModule* system = core ? core->GetModule<SystemModule>() : nullptr;
  if (!system) {
    return;
  }
  GuestMemory memory = core->Memory(isolate);
  system->Log(memory, level, message, message_length);
}

void H5vccWasmSystem::Trace(Visitor* visitor) const {
  visitor->Trace(imports_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
