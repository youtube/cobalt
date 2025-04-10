// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BYTECODE_ARRAY_INL_H_
#define V8_OBJECTS_BYTECODE_ARRAY_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/fixed-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/bytecode-array-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(BytecodeArray)

byte BytecodeArray::get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return ReadField<byte>(kHeaderSize + index * kCharSize);
}

void BytecodeArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WriteField<byte>(kHeaderSize + index * kCharSize, value);
}

void BytecodeArray::set_frame_size(int32_t frame_size) {
  DCHECK_GE(frame_size, 0);
  DCHECK(IsAligned(frame_size, kSystemPointerSize));
  WriteField<int32_t>(kFrameSizeOffset, frame_size);
}

int32_t BytecodeArray::frame_size() const {
  return ReadField<int32_t>(kFrameSizeOffset);
}

int BytecodeArray::register_count() const {
  return static_cast<int>(frame_size()) / kSystemPointerSize;
}

void BytecodeArray::set_parameter_count(int32_t number_of_parameters) {
  DCHECK_GE(number_of_parameters, 0);
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  WriteField<int32_t>(kParameterSizeOffset,
                      (number_of_parameters << kSystemPointerSizeLog2));
}

interpreter::Register BytecodeArray::incoming_new_target_or_generator_register()
    const {
  int32_t register_operand =
      ReadField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset);
  if (register_operand == 0) {
    return interpreter::Register::invalid_value();
  } else {
    return interpreter::Register::FromOperand(register_operand);
  }
}

void BytecodeArray::set_incoming_new_target_or_generator_register(
    interpreter::Register incoming_new_target_or_generator_register) {
  if (!incoming_new_target_or_generator_register.is_valid()) {
    WriteField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset, 0);
  } else {
    DCHECK(incoming_new_target_or_generator_register.index() <
           register_count());
    DCHECK_NE(0, incoming_new_target_or_generator_register.ToOperand());
    WriteField<int32_t>(kIncomingNewTargetOrGeneratorRegisterOffset,
                        incoming_new_target_or_generator_register.ToOperand());
  }
}

uint16_t BytecodeArray::bytecode_age() const {
  // Bytecode is aged by the concurrent marker.
  return RELAXED_READ_UINT16_FIELD(*this, kBytecodeAgeOffset);
}

void BytecodeArray::set_bytecode_age(uint16_t age) {
  // Bytecode is aged by the concurrent marker.
  RELAXED_WRITE_UINT16_FIELD(*this, kBytecodeAgeOffset, age);
}

int32_t BytecodeArray::parameter_count() const {
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  return ReadField<int32_t>(kParameterSizeOffset) >> kSystemPointerSizeLog2;
}

void BytecodeArray::clear_padding() {
  int data_size = kHeaderSize + length();
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

Address BytecodeArray::GetFirstBytecodeAddress() {
  return ptr() - kHeapObjectTag + kHeaderSize;
}

bool BytecodeArray::HasSourcePositionTable() const {
  Object maybe_table = source_position_table(kAcquireLoad);
  return !(maybe_table.IsUndefined() || DidSourcePositionGenerationFail());
}

bool BytecodeArray::DidSourcePositionGenerationFail() const {
  return source_position_table(kAcquireLoad).IsException();
}

void BytecodeArray::SetSourcePositionsFailedToCollect() {
  set_source_position_table(GetReadOnlyRoots().exception(), kReleaseStore);
}

DEF_GETTER(BytecodeArray, SourcePositionTable, ByteArray) {
  // WARNING: This function may be called from a background thread, hence
  // changes to how it accesses the heap can easily lead to bugs.
  Object maybe_table = source_position_table(cage_base, kAcquireLoad);
  if (maybe_table.IsByteArray(cage_base)) return ByteArray::cast(maybe_table);
  ReadOnlyRoots roots = GetReadOnlyRoots();
  DCHECK(maybe_table.IsUndefined(roots) || maybe_table.IsException(roots));
  return roots.empty_byte_array();
}

DEF_GETTER(BytecodeArray, raw_constant_pool, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kConstantPoolOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsFixedArray());
  return value;
}

DEF_GETTER(BytecodeArray, raw_handler_table, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kHandlerTableOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsByteArray());
  return value;
}

DEF_GETTER(BytecodeArray, raw_source_position_table, Object) {
  Object value =
      TaggedField<Object>::load(cage_base, *this, kSourcePositionTableOffset);
  // This field might be 0 during deserialization.
  DCHECK(value == Smi::zero() || value.IsByteArray() || value.IsUndefined() ||
         value.IsException());
  return value;
}

int BytecodeArray::BytecodeArraySize() const { return SizeFor(this->length()); }

DEF_GETTER(BytecodeArray, SizeIncludingMetadata, int) {
  int size = BytecodeArraySize();
  Object maybe_constant_pool = raw_constant_pool(cage_base);
  if (maybe_constant_pool.IsFixedArray()) {
    size += FixedArray::cast(maybe_constant_pool).Size(cage_base);
  } else {
    DCHECK_EQ(maybe_constant_pool, Smi::zero());
  }
  Object maybe_handler_table = raw_handler_table(cage_base);
  if (maybe_handler_table.IsByteArray()) {
    size += ByteArray::cast(maybe_handler_table).Size();
  } else {
    DCHECK_EQ(maybe_handler_table, Smi::zero());
  }
  Object maybe_table = raw_source_position_table(cage_base);
  if (maybe_table.IsByteArray()) {
    size += ByteArray::cast(maybe_table).Size();
  }
  return size;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BYTECODE_ARRAY_INL_H_
