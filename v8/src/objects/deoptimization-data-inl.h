// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEOPTIMIZATION_DATA_INL_H_
#define V8_OBJECTS_DEOPTIMIZATION_DATA_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/objects/deoptimization-data.h"
#include "src/objects/fixed-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(DeoptimizationData, FixedArray)

CAST_ACCESSOR(DeoptimizationData)
CAST_ACCESSOR(DeoptimizationLiteralArray)

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, TranslationArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, DeoptimizationLiteralArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
DEFINE_DEOPT_ELEMENT_ACCESSORS(DeoptExitStart, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(EagerDeoptCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LazyDeoptCount, Smi)

DEFINE_DEOPT_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)
#ifdef DEBUG
DEFINE_DEOPT_ENTRY_ACCESSORS(NodeId, Smi)
#endif  // DEBUG

BytecodeOffset DeoptimizationData::GetBytecodeOffset(int i) const {
  return BytecodeOffset(BytecodeOffsetRaw(i).value());
}

void DeoptimizationData::SetBytecodeOffset(int i, BytecodeOffset value) {
  SetBytecodeOffsetRaw(i, Smi::FromInt(value.ToInt()));
}

int DeoptimizationData::DeoptCount() {
  return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
}

inline DeoptimizationLiteralArray::DeoptimizationLiteralArray(Address ptr)
    : WeakFixedArray(ptr) {
  // No type check is possible beyond that for WeakFixedArray.
}

inline Object DeoptimizationLiteralArray::get(int index) const {
  return get(GetPtrComprCageBase(*this), index);
}

inline Object DeoptimizationLiteralArray::get(PtrComprCageBase cage_base,
                                              int index) const {
  MaybeObject maybe = Get(cage_base, index);

  // Slots in the DeoptimizationLiteralArray should only be cleared when there
  // is no possible code path that could need that slot. This works because the
  // weakly-held deoptimization literals are basically local variables that
  // TurboFan has decided not to keep on the stack. Thus, if the deoptimization
  // literal goes away, then whatever code needed it should be unreachable. The
  // exception is currently running InstructionStream: in that case, the
  // deoptimization literals array might be the only thing keeping the target
  // object alive. Thus, when a InstructionStream is running, we strongly mark
  // all of its deoptimization literals.
  CHECK(!maybe.IsCleared());

  return maybe.GetHeapObjectOrSmi();
}

inline void DeoptimizationLiteralArray::set(int index, Object value) {
  MaybeObject maybe = MaybeObject::FromObject(value);
  if (Code::IsWeakObjectInDeoptimizationLiteralArray(value)) {
    maybe = MaybeObject::MakeWeak(maybe);
  }
  Set(index, maybe);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEOPTIMIZATION_DATA_INL_H_
