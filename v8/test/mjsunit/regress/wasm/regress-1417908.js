// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction("testFailNull", makeSig([], [kWasmAnyRef]))
.exportFunc()
.addBody([
  kExprRefNull, kAnyRefCode,
  kGCPrefix, kExprBrOnCastGeneric, 0b01, 0, kAnyRefCode, kStructRefCode,
  kGCPrefix, kExprBrOnCastFailGeneric, 0b11, 0, kAnyRefCode, kNullRefCode,
  kGCPrefix, kExprBrOnCastGeneric, 0b01, 0, kAnyRefCode, kStructRefCode,
  kExprUnreachable,
]);

builder.addFunction("testNull", makeSig([], [kWasmAnyRef]))
.exportFunc()
.addBody([
  kExprRefNull, kAnyRefCode,
  kGCPrefix, kExprBrOnCastGeneric, 0b01, 0, kAnyRefCode, kStructRefCode,
  kGCPrefix, kExprBrOnCastGeneric, 0b11, 0, kAnyRefCode, kNullRefCode,
  kGCPrefix, kExprBrOnCastGeneric, 0b01, 0, kAnyRefCode, kStructRefCode,
  kExprUnreachable,
]);

let wasm = builder.instantiate().exports;
assertTraps(kTrapUnreachable, () => wasm.testFailNull());
assertSame(null, wasm.testNull());
