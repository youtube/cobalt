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
#include "cobalt/starboard_wasm/core/module.h"
#include "v8/include/v8-forward.h"

namespace cobalt {
namespace starboard_wasm {

struct BindingState;

// The Starboard Wasm imports of one execution context: owns its modules and
// event queue, builds import objects, and resolves guest pointers against the
// bound WebAssembly.Memory. Lives on the context's JS thread.
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

  v8::Local<v8::Object> NewImportObject(v8::Isolate* isolate,
                                        v8::Local<v8::Context> context);

  bool BindMemory(v8::Isolate* isolate,
                  v8::Local<v8::Value> memory,
                  std::string* error);

  // Returns the module named |name|, or nullptr (also after Shutdown()).
  Module* GetModule(const char* name) const;

  // Destroys the modules and releases the bound memory. Later import calls
  // return 0.
  void Shutdown();

  static double NowUs();

 private:
  std::shared_ptr<BindingState> state_;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_PROVIDER_CORE_H_
