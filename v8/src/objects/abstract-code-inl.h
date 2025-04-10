// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ABSTRACT_CODE_INL_H_
#define V8_OBJECTS_ABSTRACT_CODE_INL_H_

#include "src/objects/abstract-code.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/code-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(AbstractCode, HeapObject)
CAST_ACCESSOR(AbstractCode)

int AbstractCode::InstructionSize(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().instruction_size();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().length();
  }
}

ByteArray AbstractCode::SourcePositionTableInternal(
    PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    Code code = GetCode();
    if (!code.has_instruction_stream()) {
      return GetReadOnlyRoots().empty_byte_array();
    }
    return code.source_position_table(cage_base);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SourcePositionTable(cage_base);
  }
}

ByteArray AbstractCode::SourcePositionTable(Isolate* isolate,
                                            SharedFunctionInfo sfi) {
  Map map_object = map(isolate);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().SourcePositionTable(isolate, sfi);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SourcePositionTable(isolate);
  }
}

int AbstractCode::SizeIncludingMetadata(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().SizeIncludingMetadata();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().SizeIncludingMetadata();
  }
}

Address AbstractCode::InstructionStart(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().instruction_start();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return GetBytecodeArray().GetFirstBytecodeAddress();
  }
}

Address AbstractCode::InstructionEnd(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().instruction_end();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    BytecodeArray bytecode_array = GetBytecodeArray();
    return bytecode_array.GetFirstBytecodeAddress() + bytecode_array.length();
  }
}

bool AbstractCode::contains(Isolate* isolate, Address inner_pointer) {
  PtrComprCageBase cage_base(isolate);
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().contains(isolate, inner_pointer);
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return (address() <= inner_pointer) &&
           (inner_pointer <= address() + Size(cage_base));
  }
}

CodeKind AbstractCode::kind(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().kind();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return CodeKind::INTERPRETED_FUNCTION;
  }
}

Builtin AbstractCode::builtin_id(PtrComprCageBase cage_base) {
  Map map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode().builtin_id();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return Builtin::kNoBuiltinId;
  }
}

bool AbstractCode::has_instruction_stream(PtrComprCageBase cage_base) {
  DCHECK(InstanceTypeChecker::IsCode(map(cage_base)));
  return GetCode().has_instruction_stream();
}

bool AbstractCode::IsCode(PtrComprCageBase cage_base) const {
  return HeapObject::IsCode(cage_base);
}

bool AbstractCode::IsBytecodeArray(PtrComprCageBase cage_base) const {
  return HeapObject::IsBytecodeArray(cage_base);
}

Code AbstractCode::GetCode() { return Code::cast(*this); }

BytecodeArray AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(*this);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ABSTRACT_CODE_INL_H_
