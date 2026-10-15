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

// Generic marshalling between Wasm import calls and C++ module methods.
//
// A module binds a free function per import:
//
//   uint32_t PlayerCreate(PlayerModule& module, Call& call, uint32_t ptr, ...);
//   table.Add<&PlayerCreate>("sb_player", "create");
//
// The first two parameters are the module instance and a Call that resolves
// guest pointers; the remaining parameters (uint32_t, int32_t or int64_t) are
// the Wasm import's parameters. One template instantiation per import covers
// both the V8 fast API path (kHot) and the regular callback path.

#ifndef COBALT_STARBOARD_WASM_CORE_IMPORT_TABLE_H_
#define COBALT_STARBOARD_WASM_CORE_IMPORT_TABLE_H_

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/memory/stack_allocated.h"
#include "cobalt/starboard_wasm/core/module.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-fast-api-calls.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-wasm.h"

namespace cobalt {
namespace starboard_wasm {

// State shared by every import function of one execution context. Import
// functions keep it alive (V8 may call them after the context shut down, in
// which case they return 0 without touching the modules).
struct BindingState {
  BindingState();
  ~BindingState();

  std::unique_ptr<EventQueue> events;
  std::vector<std::unique_ptr<Module>> modules;
  v8::Global<v8::WasmMemoryObject> memory;
  bool shut_down = false;
};

// Per-module data attached to each import function through a v8::External,
// freed by a weak callback once V8 collects the functions. Refers to its module
// by index because modules are destroyed at shutdown, possibly long before V8
// collects the functions.
struct ImportData {
  std::shared_ptr<BindingState> state;
  size_t module_index;
  v8::Global<v8::External> handle;
};

// Resolves guest pointers against the bound WebAssembly.Memory for the
// duration of one import call. Memory can grow between calls, so the backing
// store is looked up once per call.
class Call {
  STACK_ALLOCATED();

 public:
  Call(BindingState* state, v8::Isolate* isolate)
      : state_(state), isolate_(isolate) {}

  // Returns a host pointer for [ptr, ptr + len), or nullptr if the range is
  // empty or outside guest memory.
  uint8_t* Bytes(uint32_t ptr, uint32_t len) { return Resolve(ptr, len); }

  // Like Bytes() for |count| elements of |elem_size| bytes each.
  uint8_t* Array(uint32_t ptr, uint32_t count, size_t elem_size) {
    const uint64_t bytes = static_cast<uint64_t>(count) * elem_size;
    if (bytes > std::numeric_limits<uint32_t>::max()) {
      return nullptr;
    }
    return Resolve(ptr, bytes);
  }

 private:
  uint8_t* Resolve(uint32_t ptr, uint64_t len) {
    if (len == 0 || !LoadMemory()) {
      return nullptr;
    }
    if (ptr > size_ || len > size_ - ptr) {
      return nullptr;
    }
    return base_ + ptr;
  }

  bool LoadMemory() {
    if (loaded_) {
      return base_ != nullptr;
    }
    loaded_ = true;
    if (state_->memory.IsEmpty()) {
      return false;
    }
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ArrayBuffer> buffer = state_->memory.Get(isolate_)->Buffer();
    base_ = static_cast<uint8_t*>(buffer->Data());
    size_ = buffer->ByteLength();
    return base_ != nullptr;
  }

  BindingState* const state_;
  v8::Isolate* const isolate_;
  bool loaded_ = false;
  uint8_t* base_ = nullptr;
  size_t size_ = 0;
};

namespace internal {

// Converts a JS number to int64_t like Wasm's i64.trunc_sat: NaN maps to 0
// and out-of-range values saturate.
inline int64_t SaturatingToInt64(double d) {
  if (std::isnan(d)) {
    return 0;
  }
  // 2^63 is exactly representable; every double below it fits in int64_t.
  constexpr double kTwoTo63 = 9223372036854775808.0;
  if (d >= kTwoTo63) {
    return std::numeric_limits<int64_t>::max();
  }
  if (d < -kTwoTo63) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(d);
}

template <typename T>
T FromJs(v8::Local<v8::Value> v) {
  if constexpr (std::is_same_v<T, int64_t>) {
    if (v->IsBigInt()) {
      return v.As<v8::BigInt>()->Int64Value();
    }
    if (v->IsNumber()) {
      return SaturatingToInt64(v.As<v8::Number>()->Value());
    }
    return 0;
  } else {
    if (v->IsInt32()) {
      return static_cast<T>(v.As<v8::Int32>()->Value());
    }
    if (v->IsNumber()) {
      // Wraps modulo 2^32 like a Wasm i32 parameter.
      return static_cast<T>(SaturatingToInt64(v.As<v8::Number>()->Value()));
    }
    return 0;
  }
}

inline ImportData* DataFrom(v8::Local<v8::Value> data) {
  if (data.IsEmpty() || !data->IsExternal()) {
    return nullptr;
  }
  return static_cast<ImportData*>(data.As<v8::External>()->Value());
}

template <auto Fn>
struct ImportBinding;

template <typename M, typename R, typename... Args, R (*Fn)(M&, Call&, Args...)>
struct ImportBinding<Fn> {
  static constexpr int kArity = sizeof...(Args);

  static R Invoke(ImportData* data, v8::Isolate* isolate, Args... args) {
    if (!data || data->state->shut_down) {
      return R();
    }
    Call call(data->state.get(), isolate);
    return Fn(static_cast<M&>(*data->state->modules[data->module_index]), call,
              args...);
  }

  static R Fast(v8::Local<v8::Object> /*receiver*/,
                Args... args,
                v8::FastApiCallbackOptions& options) {
    return Invoke(DataFrom(options.data),
                  options.isolate ? options.isolate : v8::Isolate::GetCurrent(),
                  args...);
  }

  template <size_t... Is>
  static void SlowImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
                       std::index_sequence<Is...>) {
    std::tuple<Args...> unpacked{FromJs<Args>(info[Is])...};
    if constexpr (std::is_void_v<R>) {
      Invoke(DataFrom(info.Data()), info.GetIsolate(),
             std::get<Is>(unpacked)...);
    } else {
      R r = Invoke(DataFrom(info.Data()), info.GetIsolate(),
                   std::get<Is>(unpacked)...);
      if constexpr (std::is_same_v<R, int64_t>) {
        info.GetReturnValue().Set(v8::BigInt::New(info.GetIsolate(), r));
      } else {
        info.GetReturnValue().Set(r);
      }
    }
  }

  static void Slow(const v8::FunctionCallbackInfo<v8::Value>& info) {
    SlowImpl(info, std::index_sequence_for<Args...>{});
  }
};

}  // namespace internal

// Collects one module's imports into the import object under construction.
class ImportTable {
  STACK_ALLOCATED();

 public:
  // Maps Wasm module names to their objects on |root|, shared by the tables
  // of every module so two modules can add to the same Wasm module.
  using WasmModuleObjects = std::map<std::string, v8::Local<v8::Object>>;

  ImportTable(v8::Isolate* isolate,
              v8::Local<v8::Context> context,
              v8::Local<v8::Object> root,
              WasmModuleObjects& wasm_modules,
              v8::Local<v8::External> data)
      : isolate_(isolate),
        context_(context),
        root_(root),
        wasm_modules_(wasm_modules),
        data_(data) {}
  ImportTable(const ImportTable&) = delete;
  ImportTable& operator=(const ImportTable&) = delete;

  // Adds `<wasm_module>.<name>` calling |Fn|. |kHot| also registers a V8 fast
  // API entry point, for imports called often (per frame or per sample).
  template <auto Fn, bool kHot = false>
  void Add(const char* wasm_module, const char* name) {
    using Binding = internal::ImportBinding<Fn>;
    const v8::CFunction c_function = v8::CFunction::Make(
        &Binding::Fast, v8::CFunctionInfo::Int64Representation::kBigInt);
    v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
        isolate_, &Binding::Slow, data_, v8::Local<v8::Signature>(),
        Binding::kArity, v8::ConstructorBehavior::kThrow,
        v8::SideEffectType::kHasSideEffect, kHot ? &c_function : nullptr);
    AddFunction(wasm_module, name, tmpl);
  }

 private:
  void AddFunction(const char* wasm_module,
                   const char* name,
                   v8::Local<v8::FunctionTemplate> tmpl);

  v8::Isolate* const isolate_;
  const v8::Local<v8::Context> context_;
  const v8::Local<v8::Object> root_;
  WasmModuleObjects& wasm_modules_;
  const v8::Local<v8::External> data_;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_IMPORT_TABLE_H_
