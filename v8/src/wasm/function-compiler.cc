// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-compiler.h"

#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/wasm-compiler.h"
#include "src/handles/handles-inl.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/log.h"
#include "src/objects/code-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-debug.h"

namespace v8::internal::wasm {

WasmCompilationResult WasmCompilationUnit::ExecuteCompilation(
    CompilationEnv* env, const WireBytesStorage* wire_bytes_storage,
    Counters* counters, AssemblerBufferCache* buffer_cache,
    WasmFeatures* detected) {
  WasmCompilationResult result;
  if (func_index_ < static_cast<int>(env->module->num_imported_functions)) {
    result = ExecuteImportWrapperCompilation(env);
  } else {
    result = ExecuteFunctionCompilation(env, wire_bytes_storage, counters,
                                        buffer_cache, detected);
  }

  if (result.succeeded() && counters) {
    counters->wasm_generated_code_size()->Increment(
        result.code_desc.instr_size);
    counters->wasm_reloc_size()->Increment(result.code_desc.reloc_size);
  }

  result.func_index = func_index_;
  result.requested_tier = tier_;

  return result;
}

WasmCompilationResult WasmCompilationUnit::ExecuteImportWrapperCompilation(
    CompilationEnv* env) {
  const FunctionSig* sig = env->module->functions[func_index_].sig;
  // Assume the wrapper is going to be a JS function with matching arity at
  // instantiation time.
  auto kind = kDefaultImportCallKind;
  bool source_positions = is_asmjs_module(env->module);
  WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
      env, kind, sig, source_positions,
      static_cast<int>(sig->parameter_count()), wasm::kNoSuspend);
  return result;
}

WasmCompilationResult WasmCompilationUnit::ExecuteFunctionCompilation(
    CompilationEnv* env, const WireBytesStorage* wire_bytes_storage,
    Counters* counters, AssemblerBufferCache* buffer_cache,
    WasmFeatures* detected) {
  auto* func = &env->module->functions[func_index_];
  base::Vector<const uint8_t> code = wire_bytes_storage->GetCode(func->code);
  wasm::FunctionBody func_body{func->sig, func->code.offset(), code.begin(),
                               code.end()};

  base::Optional<TimedHistogramScope> wasm_compile_function_time_scope;
  base::Optional<TimedHistogramScope> wasm_compile_huge_function_time_scope;
  if (counters && base::TimeTicks::IsHighResolution()) {
    if (func_body.end - func_body.start >= 100 * KB) {
      auto huge_size_histogram = SELECT_WASM_COUNTER(
          counters, env->module->origin, wasm, huge_function_size_bytes);
      huge_size_histogram->AddSample(
          static_cast<int>(func_body.end - func_body.start));
      wasm_compile_huge_function_time_scope.emplace(
          counters->wasm_compile_huge_function_time());
    }
    auto timed_histogram = SELECT_WASM_COUNTER(counters, env->module->origin,
                                               wasm_compile, function_time);
    wasm_compile_function_time_scope.emplace(timed_histogram);
  }

  // Before executing compilation, make sure that the function was validated.
  // Both Liftoff and TurboFan compilation do not perform validation, so can
  // only run on valid functions.
  if (V8_UNLIKELY(!env->module->function_was_validated(func_index_))) {
    // This code path can only be reached in
    // - eager compilation mode,
    // - with lazy validation,
    // - with PGO (which compiles some functions eagerly), or
    // - with compilation hints (which also compiles some functions eagerly).
    DCHECK(!v8_flags.wasm_lazy_compilation || v8_flags.wasm_lazy_validation ||
           v8_flags.experimental_wasm_pgo_from_file ||
           v8_flags.experimental_wasm_compilation_hints);
    if (ValidateFunctionBody(env->enabled_features, env->module, detected,
                             func_body)
            .failed()) {
      return {};
    }
    env->module->set_function_validated(func_index_);
  }

  if (v8_flags.trace_wasm_compiler) {
    PrintF("Compiling wasm function %d with %s\n", func_index_,
           ExecutionTierToString(tier_));
  }

  WasmCompilationResult result;

  switch (tier_) {
    case ExecutionTier::kNone:
      UNREACHABLE();

    case ExecutionTier::kLiftoff:
      // The --wasm-tier-mask-for-testing flag can force functions to be
      // compiled with TurboFan, and the --wasm-debug-mask-for-testing can force
      // them to be compiled for debugging, see documentation.
      if (V8_LIKELY(v8_flags.wasm_tier_mask_for_testing == 0) ||
          func_index_ >= 32 ||
          ((v8_flags.wasm_tier_mask_for_testing & (1 << func_index_)) == 0) ||
          v8_flags.liftoff_only) {
        // We do not use the debug side table, we only (optionally) pass it to
        // cover different code paths in Liftoff for testing.
        std::unique_ptr<DebugSideTable> unused_debug_sidetable;
        std::unique_ptr<DebugSideTable>* debug_sidetable_ptr = nullptr;
        if (V8_UNLIKELY(func_index_ < 32 &&
                        (v8_flags.wasm_debug_mask_for_testing &
                         (1 << func_index_)) != 0)) {
          debug_sidetable_ptr = &unused_debug_sidetable;
        }
        result = ExecuteLiftoffCompilation(
            env, func_body,
            LiftoffOptions{}
                .set_func_index(func_index_)
                .set_for_debugging(for_debugging_)
                .set_counters(counters)
                .set_detected_features(detected)
                .set_assembler_buffer_cache(buffer_cache)
                .set_debug_sidetable(debug_sidetable_ptr));
        if (result.succeeded()) break;
      }

      // If --liftoff-only, do not fall back to turbofan, even if compilation
      // failed.
      if (v8_flags.liftoff_only) break;

      // If Liftoff failed, fall back to TurboFan.
      // TODO(wasm): We could actually stop or remove the tiering unit for this
      // function to avoid compiling it twice with TurboFan.
      V8_FALLTHROUGH;

    case ExecutionTier::kTurbofan:
      compiler::WasmCompilationData data(func_body);
      data.func_index = func_index_;
      data.wire_bytes_storage = wire_bytes_storage;
      data.buffer_cache = buffer_cache;
      result = compiler::ExecuteTurbofanWasmCompilation(env, data, counters,
                                                        detected);
      result.for_debugging = for_debugging_;
      break;
  }

  DCHECK(result.succeeded());
  return result;
}

// static
void WasmCompilationUnit::CompileWasmFunction(Counters* counters,
                                              NativeModule* native_module,
                                              WasmFeatures* detected,
                                              const WasmFunction* function,
                                              ExecutionTier tier) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  FunctionBody function_body{function->sig, function->code.offset(),
                             wire_bytes.start() + function->code.offset(),
                             wire_bytes.start() + function->code.end_offset()};

  DCHECK_LE(native_module->num_imported_functions(), function->func_index);
  DCHECK_LT(function->func_index, native_module->num_functions());
  WasmCompilationUnit unit(function->func_index, tier, kNotForDebugging);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = unit.ExecuteCompilation(
      &env, native_module->compilation_state()->GetWireBytesStorage().get(),
      counters, nullptr, detected);
  if (result.succeeded()) {
    WasmCodeRefScope code_ref_scope;
    AssumptionsJournal* assumptions = result.assumptions.get();
    native_module->PublishCode(native_module->AddCompiledCode(result),
                               assumptions->empty() ? nullptr : assumptions);
  } else {
    native_module->compilation_state()->SetError();
  }
}

namespace {
bool UseGenericWrapper(const FunctionSig* sig) {
#if V8_TARGET_ARCH_ARM64
  if (!v8_flags.enable_wasm_arm64_generic_wrapper) {
    return false;
  }
#endif
#if (V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64)
  if (sig->returns().size() > 1) {
    return false;
  }
  if (sig->returns().size() == 1) {
    ValueType ret = sig->GetReturn(0);
    if (ret.kind() == kS128) return false;
    if (ret.is_reference()) {
      if (ret.heap_representation() != wasm::HeapType::kExtern &&
          ret.heap_representation() != wasm::HeapType::kFunc) {
        return false;
      }
    }
  }
  for (ValueType type : sig->parameters()) {
    if (type.kind() != kI32 && type.kind() != kI64 && type.kind() != kF32 &&
        type.kind() != kF64 &&
        // TODO(7748): The generic wrapper should also take care of null checks.
        !(type.kind() == kRefNull &&
          type.heap_representation() == wasm::HeapType::kExtern)) {
      return false;
    }
  }
  return v8_flags.wasm_generic_wrapper;
#else
  return false;
#endif
}
}  // namespace

JSToWasmWrapperCompilationUnit::JSToWasmWrapperCompilationUnit(
    Isolate* isolate, const FunctionSig* sig, uint32_t canonical_sig_index,
    const WasmModule* module, bool is_import,
    const WasmFeatures& enabled_features, AllowGeneric allow_generic)
    : isolate_(isolate),
      is_import_(is_import),
      sig_(sig),
      canonical_sig_index_(canonical_sig_index),
      use_generic_wrapper_(allow_generic && UseGenericWrapper(sig) &&
                           !is_import),
      job_(use_generic_wrapper_
               ? nullptr
               : compiler::NewJSToWasmCompilationJob(
                     isolate, sig, module, is_import, enabled_features)) {}

JSToWasmWrapperCompilationUnit::~JSToWasmWrapperCompilationUnit() = default;

void JSToWasmWrapperCompilationUnit::Execute() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileJSToWasmWrapper");
  if (!use_generic_wrapper_) {
    CompilationJob::Status status = job_->ExecuteJob(nullptr);
    CHECK_EQ(status, CompilationJob::SUCCEEDED);
  }
}

Handle<Code> JSToWasmWrapperCompilationUnit::Finalize() {
  if (use_generic_wrapper_) {
    return isolate_->builtins()->code_handle(Builtin::kGenericJSToWasmWrapper);
  }

  CompilationJob::Status status = job_->FinalizeJob(isolate_);
  CHECK_EQ(status, CompilationJob::SUCCEEDED);
  Handle<Code> code = job_->compilation_info()->code();
  if (isolate_->IsLoggingCodeCreation()) {
    Handle<String> name = isolate_->factory()->NewStringFromAsciiChecked(
        job_->compilation_info()->GetDebugName().get());
    PROFILE(isolate_, CodeCreateEvent(LogEventListener::CodeTag::kStub,
                                      Handle<AbstractCode>::cast(code), name));
  }
  return code;
}

// static
Handle<Code> JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
    Isolate* isolate, const FunctionSig* sig, uint32_t canonical_sig_index,
    const WasmModule* module, bool is_import) {
  // Run the compilation unit synchronously.
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  JSToWasmWrapperCompilationUnit unit(isolate, sig, canonical_sig_index, module,
                                      is_import, enabled_features,
                                      kAllowGeneric);
  unit.Execute();
  return unit.Finalize();
}

// static
Handle<Code> JSToWasmWrapperCompilationUnit::CompileSpecificJSToWasmWrapper(
    Isolate* isolate, const FunctionSig* sig, uint32_t canonical_sig_index,
    const WasmModule* module) {
  // Run the compilation unit synchronously.
  const bool is_import = false;
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  JSToWasmWrapperCompilationUnit unit(isolate, sig, canonical_sig_index, module,
                                      is_import, enabled_features,
                                      kDontAllowGeneric);
  unit.Execute();
  return unit.Finalize();
}

}  // namespace v8::internal::wasm
