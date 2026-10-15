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

#ifndef COBALT_STARBOARD_WASM_CORE_PROVIDER_CORE_H_
#define COBALT_STARBOARD_WASM_CORE_PROVIDER_CORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/starboard_wasm/core/guest_memory.h"
#include "cobalt/starboard_wasm/core/module.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace cobalt {
namespace starboard_wasm {

// The Starboard Wasm imports of one execution context: owns its modules and
// event queue, and the guest memory that pointer arguments refer to. Lives on
// the context's JS thread.
class ProviderCore {
 public:
  // Creates one instance of each module in |modules|. |on_wake| runs on
  // |owner| when an event arrives on an empty queue.
  ProviderCore(scoped_refptr<base::SequencedTaskRunner> owner,
               base::RepeatingClosure on_wake,
               const std::vector<ModuleFactory>& modules);

  ProviderCore(const ProviderCore&) = delete;
  ProviderCore& operator=(const ProviderCore&) = delete;

  ~ProviderCore();

  bool BindMemory(v8::Isolate* isolate,
                  v8::Local<v8::Value> memory,
                  std::string* error);

  // Resolves pointer arguments of one import call.
  GuestMemory Memory(v8::Isolate* isolate) const {
    return GuestMemory(isolate, memory_);
  }

  // Returns the module named |name|, or nullptr (also after Shutdown()).
  Module* GetModule(const char* name) const;

  template <typename T>
  T* GetModule() const {
    return static_cast<T*>(GetModule(T::kName));
  }

  // Destroys the modules and releases the bound memory.
  void Shutdown();

  static double NowUs();

 private:
  // Declared before |modules_|, which may queue events until destroyed.
  std::unique_ptr<EventQueue> events_;
  std::vector<std::unique_ptr<Module>> modules_;
  v8::Global<v8::WasmMemoryObject> memory_;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_PROVIDER_CORE_H_
