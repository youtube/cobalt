// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ABSTRACT_CODE_H_
#define V8_OBJECTS_ABSTRACT_CODE_H_

#include "src/objects/code-kind.h"
#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum class Builtin;
class BytecodeArray;
class Code;

// AbstractCode is a helper wrapper around {Code|BytecodeArray}.
// TODO(jgruber): Consider removing this wrapper as it's mainly used for
// profiling. Perhaps methods should be specialized instead of this wrapper
// class?
class AbstractCode : public HeapObject {
 public:
  int SourcePosition(PtrComprCageBase cage_base, int offset);
  int SourceStatementPosition(PtrComprCageBase cage_base, int offset);

  inline Address InstructionStart(PtrComprCageBase cage_base);
  inline Address InstructionEnd(PtrComprCageBase cage_base);
  inline int InstructionSize(PtrComprCageBase cage_base);

  // Return the source position table for interpreter code.
  inline ByteArray SourcePositionTable(Isolate* isolate,
                                       SharedFunctionInfo sfi);

  void DropStackFrameCache(PtrComprCageBase cage_base);

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata(PtrComprCageBase cage_base);

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  // Returns the kind of the code.
  inline CodeKind kind(PtrComprCageBase cage_base);

  inline Builtin builtin_id(PtrComprCageBase cage_base);

  inline bool has_instruction_stream(PtrComprCageBase cage_base);

  DECL_CAST(AbstractCode)

  inline bool IsCode(PtrComprCageBase cage_base) const;
  inline bool IsBytecodeArray(PtrComprCageBase cage_base) const;

  inline Code GetCode();
  inline BytecodeArray GetBytecodeArray();

 private:
  inline ByteArray SourcePositionTableInternal(PtrComprCageBase cage_base);

  OBJECT_CONSTRUCTORS(AbstractCode, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ABSTRACT_CODE_H_
