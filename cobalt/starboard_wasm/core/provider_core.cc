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
#include <tuple>
#include <utility>

#include "cobalt/starboard_wasm/core/event_queue.h"
#include "cobalt/starboard_wasm/core/import_table.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

void OnImportDataCollected(const v8::WeakCallbackInfo<ImportData>& info) {
  ImportData* data = info.GetParameter();
  data->handle.Reset();
  delete data;
}

}  // namespace

int64_t MonotonicNowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

BindingState::BindingState() = default;
BindingState::~BindingState() = default;

void ImportTable::AddFunction(const char* wasm_module,
                              const char* name,
                              v8::Local<v8::FunctionTemplate> tmpl) {
  v8::Local<v8::String> fn_name;
  v8::Local<v8::Function> fn;
  if (!v8::String::NewFromUtf8(isolate_, name).ToLocal(&fn_name)) {
    return;
  }
  // Wasm modules of the import object are plain objects created on first use.
  // They are tracked here rather than read back from |root_| so page script
  // (e.g. a property on Object.prototype) can't interfere.
  v8::Local<v8::Object>& module_obj = wasm_modules_[wasm_module];
  if (module_obj.IsEmpty()) {
    v8::Local<v8::String> module_key;
    if (!v8::String::NewFromUtf8(isolate_, wasm_module).ToLocal(&module_key)) {
      return;
    }
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);
    if (!root_->CreateDataProperty(context_, module_key, obj)
             .FromMaybe(false)) {
      return;
    }
    module_obj = obj;
  }
  tmpl->SetClassName(fn_name);
  if (!tmpl->GetFunction(context_).ToLocal(&fn)) {
    return;  // Execution is terminating.
  }
  fn->SetName(fn_name);
  std::ignore = module_obj->CreateDataProperty(context_, fn_name, fn);
}

ProviderCore::ProviderCore(scoped_refptr<base::SequencedTaskRunner> owner,
                           base::RepeatingClosure on_wake,
                           const std::vector<ModuleFactory>& modules)
    : state_(std::make_shared<BindingState>()) {
  state_->events = std::make_unique<EventQueue>(owner, std::move(on_wake));
  const ModuleContext context{owner, raw_ref(*state_->events)};
  for (ModuleFactory factory : modules) {
    state_->modules.push_back(factory(context));
  }
}

ProviderCore::~ProviderCore() {
  Shutdown();
}

v8::Local<v8::Object> ProviderCore::NewImportObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> root = v8::Object::New(isolate);
  if (state_->shut_down) {
    return scope.Escape(root);
  }
  ImportTable::WasmModuleObjects wasm_modules;
  for (size_t i = 0; i < state_->modules.size(); ++i) {
    auto* data = new ImportData{state_, i, {}};
    v8::Local<v8::External> external = v8::External::New(isolate, data);
    data->handle.Reset(isolate, external);
    data->handle.SetWeak(data, &OnImportDataCollected,
                         v8::WeakCallbackType::kParameter);
    ImportTable table(isolate, context, root, wasm_modules, external);
    state_->modules[i]->Bind(table);
  }
  return scope.Escape(root);
}

bool ProviderCore::BindMemory(v8::Isolate* isolate,
                              v8::Local<v8::Value> memory,
                              std::string* error) {
  if (state_->shut_down) {
    *error = "provider is shut down";
    return false;
  }
  if (memory.IsEmpty() || !memory->IsWasmMemoryObject()) {
    *error = "bindMemory() expects a WebAssembly.Memory";
    return false;
  }
  state_->memory.Reset(isolate, memory.As<v8::WasmMemoryObject>());
  return true;
}

Module* ProviderCore::GetModule(const char* name) const {
  if (state_->shut_down) {
    return nullptr;
  }
  for (const std::unique_ptr<Module>& module : state_->modules) {
    if (strcmp(module->name(), name) == 0) {
      return module.get();
    }
  }
  return nullptr;
}

void ProviderCore::Shutdown() {
  if (state_->shut_down) {
    return;
  }
  state_->shut_down = true;
  // Modules may still queue events while stopping their threads.
  state_->modules.clear();
  state_->events.reset();
  state_->memory.Reset();
}

// static
double ProviderCore::NowUs() {
  return static_cast<double>(MonotonicNowUs());
}

}  // namespace starboard_wasm
}  // namespace cobalt
