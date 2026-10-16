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

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ref.h"
#include "cobalt/starboard_wasm/core/module.h"

namespace cobalt {
namespace starboard_wasm {

class GuestMemory;

// Implements H5vccWasmSystem's event queue and logging. (Its clock needs no
// per-context state; see ProviderCore::NowUs().)
class SystemModule : public Module {
 public:
  static constexpr char kName[] = "system";

  explicit SystemModule(const ModuleContext& context);
  SystemModule(const SystemModule&) = delete;
  SystemModule& operator=(const SystemModule&) = delete;
  ~SystemModule() override;

  // Module:
  const char* name() const override;

  // Moves up to `count` queued `sb_event`s to guest memory at `events_ptr`;
  // returns how many.
  uint32_t PollEvents(GuestMemory& memory, uint32_t events_ptr, uint32_t count);
  void Log(GuestMemory& memory,
           uint32_t level,
           uint32_t message_ptr,
           uint32_t message_len);

 private:
  const raw_ref<EventQueue> events_;
};

std::unique_ptr<Module> CreateSystemModule(const ModuleContext& context);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_SYSTEM_SYSTEM_MODULE_H_
