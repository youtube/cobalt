// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/abstract-code.h"

#include "src/objects/abstract-code-inl.h"

namespace v8 {
namespace internal {

// TODO(cbruni): Move to BytecodeArray
int AbstractCode::SourcePosition(PtrComprCageBase cage_base, int offset) {
  CHECK_NE(kind(cage_base), CodeKind::BASELINE);
  Object maybe_table = SourcePositionTableInternal(cage_base);
  if (maybe_table.IsException()) return kNoSourcePosition;

  ByteArray source_position_table = ByteArray::cast(maybe_table);
  // Subtract one because the current PC is one instruction after the call site.
  if (IsCode(cage_base)) offset--;
  int position = 0;
  for (SourcePositionTableIterator iterator(
           source_position_table, SourcePositionTableIterator::kJavaScriptOnly,
           SourcePositionTableIterator::kDontSkipFunctionEntry);
       !iterator.done() && iterator.code_offset() <= offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

// TODO(cbruni): Move to BytecodeArray
int AbstractCode::SourceStatementPosition(PtrComprCageBase cage_base,
                                          int offset) {
  CHECK_NE(kind(cage_base), CodeKind::BASELINE);
  // First find the closest position.
  int position = SourcePosition(cage_base, offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  for (SourcePositionTableIterator it(SourcePositionTableInternal(cage_base));
       !it.done(); it.Advance()) {
    if (it.is_statement()) {
      int p = it.source_position().ScriptOffset();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
  }
  return statement_position;
}

}  // namespace internal
}  // namespace v8
