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
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/wasm_micro_runtime/src/core/iwasm/include/wasm_export.h"

namespace content {

namespace {

constexpr char kAbiVersionExport[] = "splash_abi_version";
constexpr char kAllocExport[] = "splash_alloc";
constexpr char kSelectExport[] = "splash_select";

// Wasm stack for the selector's calls.
constexpr uint32_t kStackSize = 16 * 1024;

// Initializes WAMR once per process. It is never torn down.
bool EnsureWamrInitialized() {
  static const bool initialized = [] {
    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_System_Allocator;
    const bool ok = wasm_runtime_full_init(&init_args);
    LOG_IF(ERROR, !ok) << "Failed to initialize WAMR.";
    return ok;
  }();
  return initialized;
}

struct ModuleDeleter {
  void operator()(WASMModuleCommon* module) { wasm_runtime_unload(module); }
};
struct InstanceDeleter {
  void operator()(WASMModuleInstanceCommon* instance) {
    wasm_runtime_deinstantiate(instance);
  }
};
struct ExecEnvDeleter {
  void operator()(WASMExecEnv* exec_env) {
    wasm_runtime_destroy_exec_env(exec_env);
  }
};

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

base::expected<std::string, std::string> RunSplashSelectorWasm(
    base::span<const uint8_t> wasm_bytes,
    std::string_view input_json,
    int instruction_limit) {
  if (input_json.size() > kMaxSplashSelectorInputSize) {
    return base::unexpected("The selector input is too large");
  }
  if (!EnsureWamrInitialized()) {
    return base::unexpected("WAMR is not available");
  }
  const uint32_t input_size = static_cast<uint32_t>(input_json.size());
  char error[128] = {};

  // 1. Load. WAMR may refer to, and modify, the buffer while the module is
  // loaded, so it gets its own copy, which outlives the module.
  std::vector<uint8_t> buffer(wasm_bytes.begin(), wasm_bytes.end());
  std::unique_ptr<WASMModuleCommon, ModuleDeleter> module(
      wasm_runtime_load(buffer.data(), static_cast<uint32_t>(buffer.size()),
                        error, sizeof(error)));
  if (!module) {
    return base::unexpected(
        base::StringPrintf("Failed to compile the selector: %s", error));
  }

  // 2. Instantiate. The selector gets no imports: it is a pure function of its
  // input.
  std::unique_ptr<WASMModuleInstanceCommon, InstanceDeleter> instance(
      wasm_runtime_instantiate(module.get(), kStackSize,
                               /*host_managed_heap_size=*/0, error,
                               sizeof(error)));
  if (!instance) {
    return base::unexpected(
        base::StringPrintf("Failed to instantiate the selector: %s", error));
  }
  std::unique_ptr<WASMExecEnv, ExecEnvDeleter> exec_env(
      wasm_runtime_create_exec_env(instance.get(), kStackSize));
  if (!exec_env) {
    return base::unexpected("Failed to create the execution environment");
  }
  wasm_runtime_set_instruction_count_limit(exec_env.get(), instruction_limit);

  auto exception = [&](const char* what) {
    const char* message = wasm_runtime_get_exception(instance.get());
    return base::unexpected(
        base::StringPrintf("%s failed: %s", what, message ? message : "?"));
  };

  // 3. Check the ABI.
  wasm_function_inst_t abi_version =
      wasm_runtime_lookup_function(instance.get(), kAbiVersionExport);
  wasm_function_inst_t alloc =
      wasm_runtime_lookup_function(instance.get(), kAllocExport);
  wasm_function_inst_t select =
      wasm_runtime_lookup_function(instance.get(), kSelectExport);
  if (!abi_version || !alloc || !select ||
      !wasm_runtime_get_default_memory(instance.get())) {
    return base::unexpected(
        "The selector doesn't export splash_abi_version, splash_alloc, "
        "splash_select and memory");
  }

  wasm_val_t results[1] = {};
  results[0].kind = WASM_I32;
  if (!wasm_runtime_call_wasm_a(exec_env.get(), abi_version, 1, results, 0,
                                nullptr)) {
    return exception("splash_abi_version()");
  }
  if (static_cast<uint32_t>(results[0].of.i32) != kSplashSelectorAbiVersion) {
    return base::unexpected(
        base::StringPrintf("Unsupported selector ABI version %u",
                           static_cast<uint32_t>(results[0].of.i32)));
  }

  // 4. Copy the input into the module's memory.
  wasm_val_t args[2] = {};
  args[0].kind = WASM_I32;
  args[0].of.i32 = static_cast<int32_t>(input_size);
  results[0].kind = WASM_I32;
  if (!wasm_runtime_call_wasm_a(exec_env.get(), alloc, 1, results, 1, args)) {
    return exception("splash_alloc()");
  }
  const uint32_t input_ptr = static_cast<uint32_t>(results[0].of.i32);
  if (input_ptr == 0 ||
      !wasm_runtime_validate_app_addr(instance.get(), input_ptr, input_size)) {
    return base::unexpected("splash_alloc() returned an invalid pointer");
  }
  memcpy(wasm_runtime_addr_app_to_native(instance.get(), input_ptr),
         input_json.data(), input_size);

  // 5. Select. The result packs the output's length in the high 32 bits and
  // its address in the low 32 bits.
  args[0].of.i32 = static_cast<int32_t>(input_ptr);
  args[1].kind = WASM_I32;
  args[1].of.i32 = static_cast<int32_t>(input_size);
  results[0].kind = WASM_I64;
  if (!wasm_runtime_call_wasm_a(exec_env.get(), select, 1, results, 2, args)) {
    return exception("splash_select()");
  }
  const uint64_t packed = static_cast<uint64_t>(results[0].of.i64);
  const uint32_t output_ptr = static_cast<uint32_t>(packed);
  const uint32_t output_size = static_cast<uint32_t>(packed >> 32);
  if (!wasm_runtime_validate_app_addr(instance.get(), output_ptr,
                                      output_size)) {
    return base::unexpected("splash_select() returned an out-of-bounds result");
  }
  std::string key(static_cast<const char*>(wasm_runtime_addr_app_to_native(
                      instance.get(), output_ptr)),
                  output_size);
  if (!IsValidSplashSelectorOutput(key)) {
    return base::unexpected("splash_select() returned an invalid key");
  }
  return base::ok(std::move(key));
}

}  // namespace content
