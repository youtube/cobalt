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

#include "cobalt/shell/browser/splash/splash_wasm_runner.h"

#include <cstring>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-wasm.h"

namespace content {

namespace {

constexpr char kAbiVersionExport[] = "splash_abi_version";
constexpr char kAllocExport[] = "splash_alloc";
constexpr char kSelectExport[] = "splash_select";
constexpr char kMemoryExport[] = "memory";

// Terminates execution in an isolate after a timeout, unless disarmed first.
// Fire() runs on a thread pool thread, so the isolate is guarded by a lock:
// once Disarm() returns, Fire() no longer touches the isolate, which can then
// be destroyed.
class Watchdog : public base::RefCountedThreadSafe<Watchdog> {
 public:
  explicit Watchdog(v8::Isolate* isolate) : isolate_(isolate) {}

  void Fire() {
    base::AutoLock lock(lock_);
    if (isolate_) {
      LOG(WARNING) << "Splash selector timed out, terminating it.";
      isolate_->TerminateExecution();
    }
  }

  void Disarm() {
    base::AutoLock lock(lock_);
    isolate_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<Watchdog>;
  ~Watchdog() = default;

  base::Lock lock_;
  v8::Isolate* isolate_ GUARDED_BY(lock_);
};

std::string ExceptionToString(v8::Isolate* isolate,
                              const v8::TryCatch& try_catch) {
  if (try_catch.HasTerminated()) {
    return "terminated (timeout)";
  }
  if (try_catch.Message().IsEmpty()) {
    return "unknown error";
  }
  std::string message;
  gin::ConvertFromV8(isolate, try_catch.Message()->Get(), &message);
  return message;
}

v8::MaybeLocal<v8::Value> GetProperty(v8::Local<v8::Context> context,
                                      v8::Local<v8::Object> object,
                                      const char* name) {
  return object->Get(context, gin::StringToV8(context->GetIsolate(), name));
}

// Returns |exports[name]| if it is a function.
v8::MaybeLocal<v8::Function> GetExportedFunction(v8::Local<v8::Context> context,
                                                 v8::Local<v8::Object> exports,
                                                 const char* name) {
  v8::Local<v8::Value> value;
  if (!GetProperty(context, exports, name).ToLocal(&value) ||
      !value->IsFunction()) {
    return {};
  }
  return value.As<v8::Function>();
}

// Returns the current bytes of the module's memory. Must be re-fetched after
// every call into the module, since the memory may have grown.
base::span<uint8_t> GetMemory(v8::Local<v8::Object> memory_object) {
  v8::Local<v8::ArrayBuffer> buffer =
      v8::WasmMemoryObject::Cast(*memory_object)->Buffer();
  return base::span(static_cast<uint8_t*>(buffer->Data()),
                    buffer->ByteLength());
}

}  // namespace

bool IsValidSplashSelectorOutput(std::string_view key) {
  if (key.empty() || key.size() > kMaxSplashSelectorOutputSize) {
    return false;
  }
  for (char c : key) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                    c == '-' || c == '/';
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool IsV8ReadyForSplashSelector() {
  // Cobalt always runs in single-process mode, where the in-process renderer
  // initializes V8. See the header.
  return gin::IsolateHolder::Initialized();
}

base::expected<std::string, std::string> RunSplashSelectorWasm(
    base::span<const uint8_t> wasm_bytes,
    std::string_view input_json,
    base::TimeDelta timeout) {
  if (!gin::IsolateHolder::Initialized()) {
    return base::unexpected("V8 is not initialized");
  }
  if (input_json.size() > kMaxSplashSelectorInputSize) {
    return base::unexpected("The selector input is too large");
  }
  const uint32_t input_size = static_cast<uint32_t>(input_json.size());

  gin::IsolateHolder holder(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            gin::IsolateHolder::kSingleThread,
                            gin::IsolateHolder::IsolateType::kUtility);
  v8::Isolate* isolate = holder.isolate();
  auto watchdog = base::MakeRefCounted<Watchdog>(isolate);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, base::BindOnce(&Watchdog::Fire, watchdog), timeout);

  // Runs the module. Returns early on any error; the watchdog is disarmed
  // below in every case, before the isolate is destroyed.
  auto run = [&]() -> base::expected<std::string, std::string> {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate);

    auto error = [&](const std::string& what) {
      if (try_catch.HasCaught() || try_catch.HasTerminated()) {
        return base::unexpected(what + ": " +
                                ExceptionToString(isolate, try_catch));
      }
      return base::unexpected(what);
    };

    // 1. Compile.
    v8::Local<v8::WasmModuleObject> module;
    if (!v8::WasmModuleObject::Compile(
             isolate, v8::MemorySpan<const uint8_t>(wasm_bytes.data(),
                                                    wasm_bytes.size()))
             .ToLocal(&module)) {
      return error("Failed to compile the selector");
    }

    // 2. Instantiate, through the WebAssembly JS API since V8 has no C++ API
    // for it. The selector gets no imports: it is a pure function of its
    // input.
    v8::Local<v8::Value> wasm_namespace;
    v8::Local<v8::Value> instance_constructor;
    if (!GetProperty(context, context->Global(), "WebAssembly")
             .ToLocal(&wasm_namespace) ||
        !wasm_namespace->IsObject() ||
        !GetProperty(context, wasm_namespace.As<v8::Object>(), "Instance")
             .ToLocal(&instance_constructor) ||
        !instance_constructor->IsFunction()) {
      return error("WebAssembly is not available");
    }
    v8::Local<v8::Value> instance_args[] = {module, v8::Object::New(isolate)};
    v8::Local<v8::Object> instance;
    if (!instance_constructor.As<v8::Function>()
             ->NewInstance(context, std::size(instance_args), instance_args)
             .ToLocal(&instance)) {
      return error("Failed to instantiate the selector");
    }
    v8::Local<v8::Value> exports_value;
    if (!GetProperty(context, instance, "exports").ToLocal(&exports_value) ||
        !exports_value->IsObject()) {
      return error("The selector has no exports");
    }
    v8::Local<v8::Object> exports = exports_value.As<v8::Object>();

    // 3. Check the ABI.
    v8::Local<v8::Function> abi_version;
    v8::Local<v8::Function> alloc;
    v8::Local<v8::Function> select;
    v8::Local<v8::Value> memory_value;
    if (!GetExportedFunction(context, exports, kAbiVersionExport)
             .ToLocal(&abi_version) ||
        !GetExportedFunction(context, exports, kAllocExport).ToLocal(&alloc) ||
        !GetExportedFunction(context, exports, kSelectExport)
             .ToLocal(&select) ||
        !GetProperty(context, exports, kMemoryExport).ToLocal(&memory_value) ||
        !memory_value->IsWasmMemoryObject()) {
      return error(
          "The selector doesn't export splash_abi_version, splash_alloc, "
          "splash_select and memory");
    }
    v8::Local<v8::Object> memory = memory_value.As<v8::Object>();

    v8::Local<v8::Value> version;
    if (!abi_version->Call(context, exports, 0, nullptr).ToLocal(&version)) {
      return error("splash_abi_version() failed");
    }
    uint32_t version_number = 0;
    if (!version->IsUint32() ||
        (version_number = version.As<v8::Uint32>()->Value()) !=
            kSplashSelectorAbiVersion) {
      return error(base::StringPrintf("Unsupported selector ABI version %u",
                                      version_number));
    }

    // 4. Copy the input into the module's memory.
    v8::Local<v8::Value> alloc_args[] = {
        v8::Integer::NewFromUnsigned(isolate, input_size)};
    v8::Local<v8::Value> input_ptr_value;
    if (!alloc->Call(context, exports, std::size(alloc_args), alloc_args)
             .ToLocal(&input_ptr_value) ||
        !input_ptr_value->IsUint32()) {
      return error("splash_alloc() failed");
    }
    const uint32_t input_ptr = input_ptr_value.As<v8::Uint32>()->Value();
    base::span<uint8_t> bytes = GetMemory(memory);
    if (input_ptr == 0 || input_ptr > bytes.size() ||
        input_size > bytes.size() - input_ptr) {
      return error("splash_alloc() returned an invalid pointer");
    }
    bytes.subspan(input_ptr, input_size)
        .copy_from(base::as_byte_span(input_json));

    // 5. Select. The result packs the output's length in the high 32 bits and
    // its address in the low 32 bits.
    v8::Local<v8::Value> select_args[] = {
        v8::Integer::NewFromUnsigned(isolate, input_ptr),
        v8::Integer::NewFromUnsigned(isolate, input_size)};
    v8::Local<v8::Value> result;
    if (!select->Call(context, exports, std::size(select_args), select_args)
             .ToLocal(&result)) {
      return error("splash_select() failed");
    }
    if (!result->IsBigInt()) {
      return error("splash_select() must return an i64");
    }
    const uint64_t packed = result.As<v8::BigInt>()->Uint64Value();
    const uint32_t output_ptr = static_cast<uint32_t>(packed);
    const uint32_t output_size = static_cast<uint32_t>(packed >> 32);
    bytes = GetMemory(memory);
    if (output_ptr > bytes.size() || output_size > bytes.size() - output_ptr) {
      return error("splash_select() returned an out-of-bounds result");
    }
    std::string key(base::as_string_view(
        base::span<const uint8_t>(bytes.subspan(output_ptr, output_size))));
    if (!IsValidSplashSelectorOutput(key)) {
      return error("splash_select() returned an invalid key");
    }
    return base::ok(std::move(key));
  };

  base::expected<std::string, std::string> result = run();
  watchdog->Disarm();
  return result;
}

}  // namespace content
