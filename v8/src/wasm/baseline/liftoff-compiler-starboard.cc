// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"

#include "src/base/optional.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler-inl.h"
// TODO(clemensb): Remove dependences on compiler stuff.
#include "src/codegen/external-reference.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/smi.h"
#include "src/tracing/trace-event.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/simd-shuffle.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {


WasmCompilationResult ExecuteLiftoffCompilation(
    AccountingAllocator* allocator, CompilationEnv* env,
    const FunctionBody& func_body, int func_index, ForDebugging for_debugging,
    Counters* counters, WasmFeatures* detected, Vector<int> breakpoints,
    std::unique_ptr<DebugSideTable>* debug_sidetable, int dead_breakpoint) {
  return WasmCompilationResult();
}

std::unique_ptr<DebugSideTable> GenerateLiftoffDebugSideTable(
    AccountingAllocator* allocator, CompilationEnv* env,
    const FunctionBody& func_body, int func_index) {
  return std::unique_ptr<DebugSideTable>();
}

}  // wasm
}  // internal
}  // v8

