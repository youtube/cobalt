// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-run-utils.h"

#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
#include "src/compiler/pipeline.h"
#include "src/diagnostics/code-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace wasm {

// Helper Functions.
bool IsSameNan(float expected, float actual) {
  // Sign is non-deterministic.
  uint32_t expected_bits = base::bit_cast<uint32_t>(expected) & ~0x80000000;
  uint32_t actual_bits = base::bit_cast<uint32_t>(actual) & ~0x80000000;
  // Some implementations convert signaling NaNs to quiet NaNs.
  return (expected_bits == actual_bits) ||
         ((expected_bits | 0x00400000) == actual_bits);
}

bool IsSameNan(double expected, double actual) {
  // Sign is non-deterministic.
  uint64_t expected_bits =
      base::bit_cast<uint64_t>(expected) & ~0x8000000000000000;
  uint64_t actual_bits = base::bit_cast<uint64_t>(actual) & ~0x8000000000000000;
  // Some implementations convert signaling NaNs to quiet NaNs.
  return (expected_bits == actual_bits) ||
         ((expected_bits | 0x0008000000000000) == actual_bits);
}

TestingModuleBuilder::TestingModuleBuilder(
    Zone* zone, ModuleOrigin origin, ManuallyImportedJSFunction* maybe_import,
    TestExecutionTier tier, RuntimeExceptionSupport exception_support,
    TestingModuleMemoryType mem_type, Isolate* isolate)
    : test_module_(std::make_shared<WasmModule>(origin)),
      isolate_(isolate ? isolate : CcTest::InitIsolateOnce()),
      enabled_features_(WasmFeatures::FromIsolate(isolate_)),
      execution_tier_(tier),
      runtime_exception_support_(exception_support) {
  WasmJs::Install(isolate_, true);
  test_module_->is_memory64 = mem_type == kMemory64;
  test_module_->untagged_globals_buffer_size = kMaxGlobalsSize;
  // The GlobalsData must be located inside the sandbox, so allocate it from the
  // ArrayBuffer allocator.
  globals_data_ = reinterpret_cast<byte*>(
      CcTest::array_buffer_allocator()->Allocate(kMaxGlobalsSize));

  uint32_t maybe_import_index = 0;
  if (maybe_import) {
    // Manually add an imported function before any other functions.
    // This must happen before the instance object is created, since the
    // instance object allocates import entries.
    maybe_import_index = AddFunction(maybe_import->sig, nullptr, kImport);
    DCHECK_EQ(0, maybe_import_index);
  }

  instance_object_ = InitInstanceObject();
  Handle<FixedArray> tables(isolate_->factory()->NewFixedArray(0));
  instance_object_->set_tables(*tables);

  if (maybe_import) {
    // Manually compile an import wrapper and insert it into the instance.
    uint32_t canonical_type_index =
        GetTypeCanonicalizer()->AddRecursiveGroup(maybe_import->sig);
    WasmImportData resolved(maybe_import->js_function, maybe_import->sig,
                            canonical_type_index);
    ImportCallKind kind = resolved.kind();
    Handle<JSReceiver> callable = resolved.callable();
    WasmImportWrapperCache::ModificationScope cache_scope(
        native_module_->import_wrapper_cache());
    WasmImportWrapperCache::CacheKey key(
        kind, canonical_type_index,
        static_cast<int>(maybe_import->sig->parameter_count()), kNoSuspend);
    auto import_wrapper = cache_scope[key];
    if (import_wrapper == nullptr) {
      CodeSpaceWriteScope write_scope(native_module_);
      import_wrapper = CompileImportWrapper(
          native_module_, isolate_->counters(), kind, maybe_import->sig,
          canonical_type_index,
          static_cast<int>(maybe_import->sig->parameter_count()), kNoSuspend,
          &cache_scope);
    }

    ImportedFunctionEntry(instance_object_, maybe_import_index)
        .SetWasmToJs(isolate_, callable, import_wrapper, resolved.suspend());
  }
}

TestingModuleBuilder::~TestingModuleBuilder() {
  // When the native module dies and is erased from the cache, it is expected to
  // have either valid bytes or no bytes at all.
  native_module_->SetWireBytes({});
  CcTest::array_buffer_allocator()->Free(globals_data_, kMaxGlobalsSize);
}

byte* TestingModuleBuilder::AddMemory(uint32_t size, SharedFlag shared) {
  CHECK(!test_module_->has_memory);
  CHECK_NULL(mem_start_);
  CHECK_EQ(0, mem_size_);
  DCHECK(!instance_object_->has_memory_object());
  uint32_t initial_pages = RoundUp(size, kWasmPageSize) / kWasmPageSize;
  uint32_t maximum_pages = (test_module_->maximum_pages != 0)
                               ? test_module_->maximum_pages
                               : initial_pages;
  test_module_->has_memory = true;
  test_module_->min_memory_size = initial_pages * kWasmPageSize;
  test_module_->max_memory_size = maximum_pages * kWasmPageSize;

  // Create the WasmMemoryObject.
  Handle<WasmMemoryObject> memory_object =
      WasmMemoryObject::New(isolate_, initial_pages, maximum_pages, shared)
          .ToHandleChecked();
  instance_object_->set_memory_object(*memory_object);

  mem_start_ =
      reinterpret_cast<byte*>(memory_object->array_buffer().backing_store());
  mem_size_ = size;
  CHECK(size == 0 || mem_start_);

  WasmMemoryObject::AddInstance(isolate_, memory_object, instance_object_);
  // TODO(wasm): Delete the following two lines when test-run-wasm will use a
  // multiple of kPageSize as memory size. At the moment, the effect of these
  // two lines is used to shrink the memory for testing purposes.
  instance_object_->SetRawMemory(mem_start_, mem_size_);
  return mem_start_;
}

uint32_t TestingModuleBuilder::AddFunction(const FunctionSig* sig,
                                           const char* name,
                                           FunctionType type) {
  if (test_module_->functions.size() == 0) {
    // TODO(titzer): Reserving space here to avoid the underlying WasmFunction
    // structs from moving.
    test_module_->functions.reserve(kMaxFunctions);
    DCHECK_NULL(test_module_->validated_functions);
    test_module_->validated_functions =
        std::make_unique<std::atomic<uint8_t>[]>((kMaxFunctions + 7) / 8);
    if (is_asmjs_module(test_module_.get())) {
      // All asm.js functions are valid by design.
      std::fill_n(test_module_->validated_functions.get(),
                  (kMaxFunctions + 7) / 8, 0xff);
    }
    test_module_->type_feedback.well_known_imports.Initialize(kMaxFunctions);
  }
  uint32_t index = static_cast<uint32_t>(test_module_->functions.size());
  test_module_->functions.push_back({sig,      // sig
                                     index,    // func_index
                                     0,        // sig_index
                                     {0, 0},   // code
                                     false,    // imported
                                     false,    // exported
                                     false});  // declared
  if (type == kImport) {
    DCHECK_EQ(0, test_module_->num_declared_functions);
    ++test_module_->num_imported_functions;
    test_module_->functions.back().imported = true;
  } else {
    ++test_module_->num_declared_functions;
  }
  DCHECK_EQ(test_module_->functions.size(),
            test_module_->num_imported_functions +
                test_module_->num_declared_functions);
  if (name) {
    base::Vector<const byte> name_vec =
        base::Vector<const byte>::cast(base::CStrVector(name));
    test_module_->lazily_generated_names.AddForTesting(
        index, {AddBytes(name_vec), static_cast<uint32_t>(name_vec.length())});
  }
  DCHECK_LT(index, kMaxFunctions);  // limited for testing.
  if (!instance_object_.is_null()) {
    Handle<FixedArray> funcs = isolate_->factory()->NewFixedArrayWithZeroes(
        static_cast<int>(test_module_->functions.size()));
    instance_object_->set_wasm_internal_functions(*funcs);
  }
  return index;
}

void TestingModuleBuilder::InitializeWrapperCache() {
  isolate_->heap()->EnsureWasmCanonicalRttsSize(
      test_module_->MaxCanonicalTypeIndex() + 1);
}

Handle<JSFunction> TestingModuleBuilder::WrapCode(uint32_t index) {
  InitializeWrapperCache();
  Handle<WasmInternalFunction> internal =
      WasmInstanceObject::GetOrCreateWasmInternalFunction(
          isolate_, instance_object(), index);
  return WasmInternalFunction::GetOrCreateExternal(internal);
}

void TestingModuleBuilder::AddIndirectFunctionTable(
    const uint16_t* function_indexes, uint32_t table_size,
    ValueType table_type) {
  Handle<WasmInstanceObject> instance = instance_object();
  uint32_t table_index = static_cast<uint32_t>(test_module_->tables.size());
  test_module_->tables.emplace_back();
  WasmTable& table = test_module_->tables.back();
  table.initial_size = table_size;
  table.maximum_size = table_size;
  table.has_maximum_size = true;
  table.type = table_type;

  {
    // Allocate the indirect function table.
    Handle<FixedArray> old_tables =
        table_index == 0
            ? isolate_->factory()->empty_fixed_array()
            : handle(instance_object_->indirect_function_tables(), isolate_);
    Handle<FixedArray> new_tables =
        isolate_->factory()->CopyFixedArrayAndGrow(old_tables, 1);
    Handle<WasmIndirectFunctionTable> table_obj =
        WasmIndirectFunctionTable::New(isolate_, table.initial_size);
    new_tables->set(table_index, *table_obj);
    instance_object_->set_indirect_function_tables(*new_tables);
  }

  WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
      instance_object(), table_index, table_size);
  Handle<WasmTableObject> table_obj = WasmTableObject::New(
      isolate_, instance, table.type, table.initial_size,
      table.has_maximum_size, table.maximum_size, nullptr,
      IsSubtypeOf(table.type, kWasmExternRef, test_module_.get())
          ? Handle<Object>::cast(isolate_->factory()->null_value())
          : Handle<Object>::cast(isolate_->factory()->wasm_null()));

  WasmTableObject::AddDispatchTable(isolate_, table_obj, instance_object_,
                                    table_index);

  if (function_indexes) {
    for (uint32_t i = 0; i < table_size; ++i) {
      WasmFunction& function = test_module_->functions[function_indexes[i]];
      int sig_id =
          test_module_->isorecursive_canonical_type_ids[function.sig_index];
      FunctionTargetAndRef entry(instance, function.func_index);
      instance->GetIndirectFunctionTable(isolate_, table_index)
          ->Set(i, sig_id, entry.call_target(), *entry.ref());
      WasmTableObject::SetFunctionTablePlaceholder(
          isolate_, table_obj, i, instance_object_, function_indexes[i]);
    }
  }

  Handle<FixedArray> old_tables(instance_object_->tables(), isolate_);
  Handle<FixedArray> new_tables =
      isolate_->factory()->CopyFixedArrayAndGrow(old_tables, 1);
  new_tables->set(old_tables->length(), *table_obj);
  instance_object_->set_tables(*new_tables);
}

uint32_t TestingModuleBuilder::AddBytes(base::Vector<const byte> bytes) {
  base::Vector<const uint8_t> old_bytes = native_module_->wire_bytes();
  uint32_t old_size = static_cast<uint32_t>(old_bytes.size());
  // Avoid placing strings at offset 0, this might be interpreted as "not
  // set", e.g. for function names.
  uint32_t bytes_offset = old_size ? old_size : 1;
  size_t new_size = bytes_offset + bytes.size();
  base::OwnedVector<uint8_t> new_bytes =
      base::OwnedVector<uint8_t>::New(new_size);
  if (old_size > 0) {
    memcpy(new_bytes.begin(), old_bytes.begin(), old_size);
  } else {
    // Set the unused byte. It is never decoded, but the bytes are used as the
    // key in the native module cache.
    new_bytes[0] = 0;
  }
  memcpy(new_bytes.begin() + bytes_offset, bytes.begin(), bytes.length());
  native_module_->SetWireBytes(std::move(new_bytes));
  return bytes_offset;
}

uint32_t TestingModuleBuilder::AddException(const FunctionSig* sig) {
  DCHECK_EQ(0, sig->return_count());
  uint32_t index = static_cast<uint32_t>(test_module_->tags.size());
  test_module_->tags.emplace_back(sig, AddSignature(sig));
  Handle<WasmExceptionTag> tag = WasmExceptionTag::New(isolate_, index);
  Handle<FixedArray> table(instance_object_->tags_table(), isolate_);
  table = isolate_->factory()->CopyFixedArrayAndGrow(table, 1);
  instance_object_->set_tags_table(*table);
  table->set(index, *tag);
  return index;
}

uint32_t TestingModuleBuilder::AddPassiveDataSegment(
    base::Vector<const byte> bytes) {
  uint32_t index = static_cast<uint32_t>(test_module_->data_segments.size());
  DCHECK_EQ(index, test_module_->data_segments.size());
  DCHECK_EQ(index, data_segment_starts_.size());
  DCHECK_EQ(index, data_segment_sizes_.size());

  // Add a passive data segment. This isn't used by function compilation, but
  // but it keeps the index in sync. The data segment's source will not be
  // correct, since we don't store data in the module wire bytes.
  test_module_->data_segments.emplace_back();

  // The num_declared_data_segments (from the DataCount section) is used
  // to validate the segment index, during function compilation.
  test_module_->num_declared_data_segments = index + 1;

  Address old_data_address =
      reinterpret_cast<Address>(data_segment_data_.data());
  size_t old_data_size = data_segment_data_.size();
  data_segment_data_.resize(old_data_size + bytes.length());
  Address new_data_address =
      reinterpret_cast<Address>(data_segment_data_.data());

  memcpy(data_segment_data_.data() + old_data_size, bytes.begin(),
         bytes.length());

  // The data_segment_data_ offset may have moved, so update all the starts.
  for (Address& start : data_segment_starts_) {
    start += new_data_address - old_data_address;
  }
  data_segment_starts_.push_back(new_data_address + old_data_size);
  data_segment_sizes_.push_back(bytes.length());

  // The vector pointers may have moved, so update the instance object.
  uint32_t size = static_cast<uint32_t>(data_segment_sizes_.size());
  Handle<FixedAddressArray> data_segment_starts =
      FixedAddressArray::New(isolate_, size);
  data_segment_starts->copy_in(
      0, reinterpret_cast<byte*>(data_segment_starts_.data()),
      size * sizeof(Address));
  instance_object_->set_data_segment_starts(*data_segment_starts);
  Handle<FixedUInt32Array> data_segment_sizes =
      FixedUInt32Array::New(isolate_, size);
  data_segment_sizes->copy_in(
      0, reinterpret_cast<byte*>(data_segment_sizes_.data()),
      size * sizeof(uint32_t));
  instance_object_->set_data_segment_sizes(*data_segment_sizes);
  return index;
}

CompilationEnv TestingModuleBuilder::CreateCompilationEnv() {
  return {test_module_.get(), native_module_->bounds_checks(),
          runtime_exception_support_, enabled_features_, kNoDynamicTiering};
}

const WasmGlobal* TestingModuleBuilder::AddGlobal(ValueType type) {
  byte size = type.value_kind_size();
  global_offset = (global_offset + size - 1) & ~(size - 1);  // align
  test_module_->globals.push_back(
      {type, true, {}, {global_offset}, false, false});
  global_offset += size;
  // limit number of globals.
  CHECK_LT(global_offset, kMaxGlobalsSize);
  return &test_module_->globals.back();
}

Handle<WasmInstanceObject> TestingModuleBuilder::InitInstanceObject() {
  const bool kUsesLiftoff = true;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          test_module_.get(), kUsesLiftoff,
          DynamicTiering{v8_flags.wasm_dynamic_tiering.value()});
  auto native_module = GetWasmEngine()->NewNativeModule(
      isolate_, enabled_features_, test_module_, code_size_estimate);
  native_module->SetWireBytes(base::OwnedVector<const uint8_t>());
  native_module->compilation_state()->set_compilation_id(0);
  constexpr base::Vector<const char> kNoSourceUrl{"", 0};
  Handle<Script> script =
      GetWasmEngine()->GetOrCreateScript(isolate_, native_module, kNoSourceUrl);

  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate_, std::move(native_module), script);
  native_module_ = module_object->native_module();
  native_module_->ReserveCodeTableForTesting(kMaxFunctions);

  auto instance = WasmInstanceObject::New(isolate_, module_object);
  instance->set_tags_table(ReadOnlyRoots{isolate_}.empty_fixed_array());
  instance->set_globals_start(globals_data_);
  Handle<FixedArray> feedback_vector =
      isolate_->factory()->NewFixedArrayWithZeroes(kMaxFunctions);
  instance->set_feedback_vectors(*feedback_vector);
  return instance;
}

void TestBuildingGraphWithBuilder(compiler::WasmGraphBuilder* builder,
                                  Zone* zone, const FunctionSig* sig,
                                  const byte* start, const byte* end) {
  WasmFeatures unused_detected_features;
  FunctionBody body(sig, 0, start, end);
  std::vector<compiler::WasmLoopInfo> loops;
  BuildTFGraph(zone->allocator(), WasmFeatures::All(), nullptr, builder,
               &unused_detected_features, body, &loops, nullptr, nullptr, 0,
               nullptr, kRegularFunction);
  builder->LowerInt64(compiler::WasmGraphBuilder::kCalledFromWasm);
}

void TestBuildingGraph(Zone* zone, compiler::JSGraph* jsgraph,
                       CompilationEnv* env, const FunctionSig* sig,
                       compiler::SourcePositionTable* source_position_table,
                       const byte* start, const byte* end) {
  compiler::WasmGraphBuilder builder(env, zone, jsgraph, sig,
                                     source_position_table);
  TestBuildingGraphWithBuilder(&builder, zone, sig, start, end);
}

WasmFunctionWrapper::WasmFunctionWrapper(Zone* zone, int num_params)
    : GraphAndBuilders(zone),
      inner_code_node_(nullptr),
      context_address_(nullptr),
      signature_(nullptr) {
  // One additional parameter for the pointer to the return value memory.
  Signature<MachineType>::Builder sig_builder(zone, 1, num_params + 1);

  sig_builder.AddReturn(MachineType::Int32());
  for (int i = 0; i < num_params + 1; i++) {
    sig_builder.AddParam(MachineType::Pointer());
  }
  signature_ = sig_builder.Build();
}

void WasmFunctionWrapper::Init(CallDescriptor* call_descriptor,
                               MachineType return_type,
                               base::Vector<MachineType> param_types) {
  DCHECK_NOT_NULL(call_descriptor);
  DCHECK_EQ(signature_->parameter_count(), param_types.length() + 1);

  // Create the TF graph for the wrapper.

  // Function, context_address, effect, and control.
  Node** parameters = zone()->NewArray<Node*>(param_types.length() + 4);
  int start_value_output_count =
      static_cast<int>(signature_->parameter_count()) + 1;
  graph()->SetStart(
      graph()->NewNode(common()->Start(start_value_output_count)));
  Node* effect = graph()->start();
  int parameter_count = 0;

  // Dummy node which gets replaced in SetInnerCode.
  inner_code_node_ = graph()->NewNode(common()->Int32Constant(0));
  parameters[parameter_count++] = inner_code_node_;

  // Dummy node that gets replaced in SetContextAddress.
  context_address_ = graph()->NewNode(IntPtrConstant(0));
  parameters[parameter_count++] = context_address_;

  int param_idx = 0;
  for (MachineType t : param_types) {
    DCHECK_NE(MachineType::None(), t);
    parameters[parameter_count] = graph()->NewNode(
        machine()->Load(t),
        graph()->NewNode(common()->Parameter(param_idx++), graph()->start()),
        graph()->NewNode(common()->Int32Constant(0)), effect, graph()->start());
    effect = parameters[parameter_count++];
  }

  parameters[parameter_count++] = effect;
  parameters[parameter_count++] = graph()->start();
  const compiler::Operator* call_op = common()->Call(call_descriptor);
  // The following code assumes the call node has effect and control inputs and
  // outputs.
  DCHECK_GT(call_op->EffectInputCount(), 0);
  DCHECK_GT(call_op->EffectOutputCount(), 0);
  DCHECK_GT(call_op->ControlInputCount(), 0);
  DCHECK_GT(call_op->ControlOutputCount(), 0);

  Node* call = graph()->NewNode(call_op, parameter_count, parameters);

  if (!return_type.IsNone()) {
    effect = graph()->NewNode(
        machine()->Store(compiler::StoreRepresentation(
            return_type.representation(),
            compiler::WriteBarrierKind::kNoWriteBarrier)),
        graph()->NewNode(common()->Parameter(param_types.length()),
                         graph()->start()),
        graph()->NewNode(common()->Int32Constant(0)), call, call, call);
  }
  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* r = graph()->NewNode(
      common()->Return(), zero,
      graph()->NewNode(common()->Int32Constant(WASM_WRAPPER_RETURN_VALUE)),
      effect, call);
  graph()->SetEnd(graph()->NewNode(common()->End(1), r));
}

Handle<Code> WasmFunctionWrapper::GetWrapperCode(Isolate* isolate) {
  Handle<Code> code;
  if (!code_.ToHandle(&code)) {
    auto call_descriptor = compiler::Linkage::GetSimplifiedCDescriptor(
        zone(), signature_, CallDescriptor::kInitializeRootRegister);

    if (kSystemPointerSize == 4) {
      size_t num_params = signature_->parameter_count();
      // One additional parameter for the pointer of the return value.
      Signature<MachineRepresentation>::Builder rep_builder(zone(), 1,
                                                            num_params + 1);

      rep_builder.AddReturn(MachineRepresentation::kWord32);
      for (size_t i = 0; i < num_params + 1; i++) {
        rep_builder.AddParam(MachineRepresentation::kWord32);
      }
      compiler::Int64Lowering r(graph(), machine(), common(), simplified(),
                                zone(), rep_builder.Build());
      r.LowerGraph();
    }

    OptimizedCompilationInfo info(base::ArrayVector("testing"), graph()->zone(),
                                  CodeKind::C_WASM_ENTRY);
    code_ = compiler::Pipeline::GenerateCodeForTesting(
        &info, isolate, call_descriptor, graph(),
        AssemblerOptions::Default(isolate));
    code = code_.ToHandleChecked();
#ifdef ENABLE_DISASSEMBLER
    if (v8_flags.print_opt_code) {
      CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
      OFStream os(tracing_scope.file());

      code->Disassemble("wasm wrapper", os, isolate);
    }
#endif
  }

  return code;
}

// This struct is just a type tag for Zone::NewArray<T>(size_t) call.
struct WasmFunctionCompilerBuffer {};

void WasmFunctionCompiler::Build(base::Vector<const uint8_t> bytes) {
  size_t locals_size = local_decls.Size();
  size_t total_size = bytes.size() + locals_size + 1;
  byte* buffer = zone()->NewArray<byte, WasmFunctionCompilerBuffer>(total_size);
  // Prepend the local decls to the code.
  local_decls.Emit(buffer);
  // Emit the code.
  memcpy(buffer + locals_size, bytes.begin(), bytes.size());
  // Append an extra end opcode.
  buffer[total_size - 1] = kExprEnd;

  bytes = base::VectorOf(buffer, total_size);

  function_->code = {builder_->AddBytes(bytes),
                     static_cast<uint32_t>(bytes.size())};

  base::Vector<const uint8_t> wire_bytes = builder_->instance_object()
                                               ->module_object()
                                               .native_module()
                                               ->wire_bytes();

  CompilationEnv env = builder_->CreateCompilationEnv();
  base::ScopedVector<uint8_t> func_wire_bytes(function_->code.length());
  memcpy(func_wire_bytes.begin(), wire_bytes.begin() + function_->code.offset(),
         func_wire_bytes.length());

  FunctionBody func_body{function_->sig, function_->code.offset(),
                         func_wire_bytes.begin(), func_wire_bytes.end()};
  NativeModule* native_module =
      builder_->instance_object()->module_object().native_module();
  ForDebugging for_debugging =
      native_module->IsInDebugState() ? kForDebugging : kNotForDebugging;

  WasmFeatures unused_detected_features;
  // Validate Wasm modules; asm.js is assumed to be always valid.
  if (env.module->origin == kWasmOrigin) {
    DecodeResult validation_result = ValidateFunctionBody(
        env.enabled_features, env.module, &unused_detected_features, func_body);
    if (validation_result.failed()) {
      FATAL("Validation failed: %s",
            validation_result.error().message().c_str());
    }
    env.module->set_function_validated(function_->func_index);
  }

  base::Optional<WasmCompilationResult> result;
  if (builder_->test_execution_tier() ==
      TestExecutionTier::kLiftoffForFuzzing) {
    result.emplace(ExecuteLiftoffCompilation(
        &env, func_body,
        LiftoffOptions{}
            .set_func_index(function_->func_index)
            .set_for_debugging(kForDebugging)
            .set_max_steps(builder_->max_steps_ptr())
            .set_nondeterminism(builder_->non_determinism_ptr())));
  } else {
    WasmCompilationUnit unit(function_->func_index, builder_->execution_tier(),
                             for_debugging);
    result.emplace(unit.ExecuteCompilation(
        &env, native_module->compilation_state()->GetWireBytesStorage().get(),
        nullptr, nullptr, &unused_detected_features));
  }
  CHECK(result->succeeded());
  WasmCode* code =
      native_module->PublishCode(native_module->AddCompiledCode(*result));
  DCHECK_NOT_NULL(code);
  DisallowGarbageCollection no_gc;
  Script script = builder_->instance_object()->module_object().script();
  std::unique_ptr<char[]> source_url = String::cast(script.name()).ToCString();
  if (WasmCode::ShouldBeLogged(isolate())) {
    code->LogCode(isolate(), source_url.get(), script.id());
  }
}

WasmFunctionCompiler::WasmFunctionCompiler(Zone* zone, const FunctionSig* sig,
                                           TestingModuleBuilder* builder,
                                           const char* name)
    : GraphAndBuilders(zone),
      jsgraph(builder->isolate(), this->graph(), this->common(), nullptr,
              nullptr, this->machine()),
      sig(sig),
      descriptor_(nullptr),
      builder_(builder),
      local_decls(zone, sig),
      source_position_table_(this->graph()) {
  // Get a new function from the testing module.
  int index = builder->AddFunction(sig, name, TestingModuleBuilder::kWasm);
  function_ = builder_->GetFunctionAt(index);
}

WasmFunctionCompiler::~WasmFunctionCompiler() = default;

/* static */
FunctionSig* WasmRunnerBase::CreateSig(Zone* zone, MachineType return_type,
                                       base::Vector<MachineType> param_types) {
  int return_count = return_type.IsNone() ? 0 : 1;
  int param_count = param_types.length();

  // Allocate storage array in zone.
  ValueType* sig_types = zone->NewArray<ValueType>(return_count + param_count);

  // Convert machine types to local types, and check that there are no
  // MachineType::None()'s in the parameters.
  int idx = 0;
  if (return_count) sig_types[idx++] = ValueType::For(return_type);
  for (MachineType param : param_types) {
    CHECK_NE(MachineType::None(), param);
    sig_types[idx++] = ValueType::For(param);
  }
  return zone->New<FunctionSig>(return_count, param_count, sig_types);
}

// static
bool WasmRunnerBase::trap_happened;

}  // namespace wasm
}  // namespace internal
}  // namespace v8
