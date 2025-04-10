// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_INL_H_
#define V8_OBJECTS_CODE_INL_H_

#include "src/baseline/bytecode-offset-iterator.h"
#include "src/codegen/code-desc.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code.h"
#include "src/objects/deoptimization-data-inl.h"
#include "src/objects/instruction-stream-inl.h"
#include "src/snapshot/embedded/embedded-data-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Code, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(GcSafeCode, HeapObject)

CAST_ACCESSOR(GcSafeCode)
CAST_ACCESSOR(Code)

Code GcSafeCode::UnsafeCastToCode() const {
  return Code::unchecked_cast(*this);
}

#define GCSAFE_CODE_FWD_ACCESSOR(ReturnType, Name) \
  ReturnType GcSafeCode::Name() const { return UnsafeCastToCode().Name(); }
GCSAFE_CODE_FWD_ACCESSOR(Address, instruction_start)
GCSAFE_CODE_FWD_ACCESSOR(Address, instruction_end)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_builtin)
GCSAFE_CODE_FWD_ACCESSOR(Builtin, builtin_id)
GCSAFE_CODE_FWD_ACCESSOR(CodeKind, kind)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_interpreter_trampoline_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_baseline_trampoline_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_baseline_leave_frame_builtin)
GCSAFE_CODE_FWD_ACCESSOR(bool, has_instruction_stream)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_maglevved)
GCSAFE_CODE_FWD_ACCESSOR(bool, is_turbofanned)
GCSAFE_CODE_FWD_ACCESSOR(bool, has_tagged_outgoing_params)
GCSAFE_CODE_FWD_ACCESSOR(bool, marked_for_deoptimization)
GCSAFE_CODE_FWD_ACCESSOR(Object, raw_instruction_stream)
GCSAFE_CODE_FWD_ACCESSOR(int, stack_slots)
GCSAFE_CODE_FWD_ACCESSOR(Address, constant_pool)
GCSAFE_CODE_FWD_ACCESSOR(Address, safepoint_table_address)
#undef GCSAFE_CODE_FWD_ACCESSOR

int GcSafeCode::GetOffsetFromInstructionStart(Isolate* isolate,
                                              Address pc) const {
  return UnsafeCastToCode().GetOffsetFromInstructionStart(isolate, pc);
}

Address GcSafeCode::InstructionStart(Isolate* isolate, Address pc) const {
  return UnsafeCastToCode().InstructionStart(isolate, pc);
}

Address GcSafeCode::InstructionEnd(Isolate* isolate, Address pc) const {
  return UnsafeCastToCode().InstructionEnd(isolate, pc);
}

Address GcSafeCode::constant_pool(InstructionStream istream) const {
  return UnsafeCastToCode().constant_pool(istream);
}

bool GcSafeCode::CanDeoptAt(Isolate* isolate, Address pc) const {
  DeoptimizationData deopt_data = DeoptimizationData::unchecked_cast(
      UnsafeCastToCode().unchecked_deoptimization_data());
  Address code_start_address = instruction_start();
  for (int i = 0; i < deopt_data.DeoptCount(); i++) {
    if (deopt_data.Pc(i).value() == -1) continue;
    Address address = code_start_address + deopt_data.Pc(i).value();
    if (address == pc &&
        deopt_data.GetBytecodeOffset(i) != BytecodeOffset::None()) {
      return true;
    }
  }
  return false;
}

Object GcSafeCode::raw_instruction_stream(
    PtrComprCageBase code_cage_base) const {
  return UnsafeCastToCode().raw_instruction_stream(code_cage_base);
}

INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, metadata_size, kMetadataSizeOffset)
INT_ACCESSORS(Code, handler_table_offset, kHandlerTableOffsetOffset)
INT_ACCESSORS(Code, code_comments_offset, kCodeCommentsOffsetOffset)
INT32_ACCESSORS(Code, unwinding_info_offset, kUnwindingInfoOffsetOffset)
ACCESSORS_CHECKED2(Code, deoptimization_data, FixedArray,
                   kDeoptimizationDataOrInterpreterDataOffset,
                   kind() != CodeKind::BASELINE,
                   kind() != CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, bytecode_or_interpreter_data, HeapObject,
                   kDeoptimizationDataOrInterpreterDataOffset,
                   kind() == CodeKind::BASELINE,
                   kind() == CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, source_position_table, ByteArray, kPositionTableOffset,
                   kind() != CodeKind::BASELINE,
                   kind() != CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))
ACCESSORS_CHECKED2(Code, bytecode_offset_table, ByteArray, kPositionTableOffset,
                   kind() == CodeKind::BASELINE,
                   kind() == CodeKind::BASELINE &&
                       !ObjectInYoungGeneration(value))

ByteArray Code::SourcePositionTable(Isolate* isolate,
                                    SharedFunctionInfo sfi) const {
  if (!has_instruction_stream()) {
    return GetReadOnlyRoots().empty_byte_array();
  }

  DisallowGarbageCollection no_gc;
  if (kind() == CodeKind::BASELINE) {
    return sfi.GetBytecodeArray(isolate).SourcePositionTable(isolate);
  }
  return source_position_table(isolate);
}

Address Code::body_start() const { return instruction_start(); }

Address Code::body_end() const { return body_start() + body_size(); }

int Code::body_size() const { return instruction_size() + metadata_size(); }

Address Code::instruction_end() const {
  return instruction_start() + instruction_size();
}

Address Code::metadata_start() const {
  if (has_instruction_stream()) {
    static_assert(InstructionStream::kOnHeapBodyIsContiguous);
    return instruction_start() + instruction_size();
  }
  // An embedded builtin. Remapping is irrelevant wrt the metadata section so
  // we can simply use the global blob.
  // TODO(jgruber): Consider adding this as a physical Code field to avoid the
  // lookup. Alternatively, rename this (and callers) to camel-case to clarify
  // it's more than a simple accessor.
  static_assert(!InstructionStream::kOffHeapBodyIsContiguous);
  return EmbeddedData::FromBlob().MetadataStartOf(builtin_id());
}

Address Code::InstructionStart(Isolate* isolate, Address pc) const {
  if (V8_LIKELY(has_instruction_stream())) return instruction_start();
  // Note we intentionally don't bounds-check that `pc` is within the returned
  // instruction area.
  return EmbeddedData::FromBlobForPc(isolate, pc)
      .InstructionStartOf(builtin_id());
}

Address Code::InstructionEnd(Isolate* isolate, Address pc) const {
  return InstructionStart(isolate, pc) + instruction_size();
}

int Code::GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const {
  const Address offset = pc - InstructionStart(isolate, pc);
  DCHECK_LE(offset, instruction_size());
  return static_cast<int>(offset);
}

Address Code::metadata_end() const {
  return metadata_start() + metadata_size();
}

Address Code::safepoint_table_address() const {
  return metadata_start() + safepoint_table_offset();
}

int Code::safepoint_table_size() const {
  return handler_table_offset() - safepoint_table_offset();
}

bool Code::has_safepoint_table() const { return safepoint_table_size() > 0; }

Address Code::handler_table_address() const {
  return metadata_start() + handler_table_offset();
}

int Code::handler_table_size() const {
  return constant_pool_offset() - handler_table_offset();
}

bool Code::has_handler_table() const { return handler_table_size() > 0; }

int Code::constant_pool_size() const {
  const int size = code_comments_offset() - constant_pool_offset();
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    DCHECK_EQ(size, 0);
    return 0;
  }
  DCHECK_GE(size, 0);
  return size;
}

bool Code::has_constant_pool() const { return constant_pool_size() > 0; }

FixedArray Code::unchecked_deoptimization_data() const {
  return FixedArray::unchecked_cast(
      TaggedField<HeapObject, kDeoptimizationDataOrInterpreterDataOffset>::load(
          *this));
}

byte* Code::relocation_start() const {
  return V8_LIKELY(has_instruction_stream())
             ? instruction_stream().relocation_start()
             : nullptr;
}

byte* Code::relocation_end() const {
  return V8_LIKELY(has_instruction_stream())
             ? instruction_stream().relocation_end()
             : nullptr;
}

int Code::relocation_size() const {
  return V8_LIKELY(has_instruction_stream())
             ? instruction_stream().relocation_size()
             : 0;
}

bool Code::contains(Isolate* isolate, Address inner_pointer) const {
  const Address start = InstructionStart(isolate, inner_pointer);
  if (inner_pointer < start) return false;
  return inner_pointer < start + instruction_size();
}

int Code::InstructionStreamObjectSize() const {
  return InstructionStream::SizeFor(body_size());
}

int Code::SizeIncludingMetadata() const {
  int size = InstructionStreamObjectSize();
  size += relocation_size();
  if (kind() != CodeKind::BASELINE) {
    size += deoptimization_data().Size();
  }
  return size;
}

CodeKind Code::kind() const { return KindField::decode(flags(kRelaxedLoad)); }

int Code::GetBytecodeOffsetForBaselinePC(Address baseline_pc,
                                         BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK(!is_baseline_trampoline_builtin());
  if (is_baseline_leave_frame_builtin()) return kFunctionExitBytecodeOffset;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  Address pc = baseline_pc - instruction_start();
  offset_iterator.AdvanceToPCOffset(pc);
  return offset_iterator.current_bytecode_offset();
}

uintptr_t Code::GetBaselinePCForBytecodeOffset(int bytecode_offset,
                                               BytecodeToPCPosition position,
                                               BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  offset_iterator.AdvanceToBytecodeOffset(bytecode_offset);
  uintptr_t pc = 0;
  if (position == kPcAtStartOfBytecode) {
    pc = offset_iterator.current_pc_start_offset();
  } else {
    DCHECK_EQ(position, kPcAtEndOfBytecode);
    pc = offset_iterator.current_pc_end_offset();
  }
  return pc;
}

uintptr_t Code::GetBaselineStartPCForBytecodeOffset(int bytecode_offset,
                                                    BytecodeArray bytecodes) {
  return GetBaselinePCForBytecodeOffset(bytecode_offset, kPcAtStartOfBytecode,
                                        bytecodes);
}

uintptr_t Code::GetBaselineEndPCForBytecodeOffset(int bytecode_offset,
                                                  BytecodeArray bytecodes) {
  return GetBaselinePCForBytecodeOffset(bytecode_offset, kPcAtEndOfBytecode,
                                        bytecodes);
}

uintptr_t Code::GetBaselinePCForNextExecutedBytecode(int bytecode_offset,
                                                     BytecodeArray bytecodes) {
  DisallowGarbageCollection no_gc;
  CHECK_EQ(kind(), CodeKind::BASELINE);
  baseline::BytecodeOffsetIterator offset_iterator(
      ByteArray::cast(bytecode_offset_table()), bytecodes);
  Handle<BytecodeArray> bytecodes_handle(
      reinterpret_cast<Address*>(&bytecodes));
  interpreter::BytecodeArrayIterator bytecode_iterator(bytecodes_handle,
                                                       bytecode_offset);
  interpreter::Bytecode bytecode = bytecode_iterator.current_bytecode();
  if (bytecode == interpreter::Bytecode::kJumpLoop) {
    return GetBaselineStartPCForBytecodeOffset(
        bytecode_iterator.GetJumpTargetOffset(), bytecodes);
  } else {
    DCHECK(!interpreter::Bytecodes::IsJump(bytecode));
    DCHECK(!interpreter::Bytecodes::IsSwitch(bytecode));
    DCHECK(!interpreter::Bytecodes::Returns(bytecode));
    return GetBaselineEndPCForBytecodeOffset(bytecode_offset, bytecodes);
  }
}

inline bool Code::checks_tiering_state() const {
  bool checks_state = (builtin_id() == Builtin::kCompileLazy ||
                       builtin_id() == Builtin::kInterpreterEntryTrampoline ||
                       CodeKindCanTierUp(kind()));
  return checks_state ||
         (CodeKindCanDeoptimize(kind()) && marked_for_deoptimization());
}

inline constexpr bool CodeKindHasTaggedOutgoingParams(CodeKind kind) {
  return kind != CodeKind::JS_TO_WASM_FUNCTION &&
         kind != CodeKind::C_WASM_ENTRY && kind != CodeKind::WASM_FUNCTION;
}

inline bool Code::has_tagged_outgoing_params() const {
#if V8_ENABLE_WEBASSEMBLY
  return CodeKindHasTaggedOutgoingParams(kind()) &&
         builtin_id() != Builtin::kWasmCompileLazy;
#else
  return CodeKindHasTaggedOutgoingParams(kind());
#endif
}

inline bool Code::is_turbofanned() const {
  return IsTurbofannedField::decode(flags(kRelaxedLoad));
}

inline bool Code::is_maglevved() const { return kind() == CodeKind::MAGLEV; }

unsigned Code::inlined_bytecode_size() const {
  unsigned size = RELAXED_READ_UINT_FIELD(*this, kInlinedBytecodeSizeOffset);
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  return size;
}

void Code::set_inlined_bytecode_size(unsigned size) {
  DCHECK(CodeKindIsOptimizedJSFunction(kind()) || size == 0);
  RELAXED_WRITE_UINT_FIELD(*this, kInlinedBytecodeSizeOffset, size);
}

BytecodeOffset Code::osr_offset() const {
  return BytecodeOffset(RELAXED_READ_INT32_FIELD(*this, kOsrOffsetOffset));
}

void Code::set_osr_offset(BytecodeOffset offset) {
  RELAXED_WRITE_INT32_FIELD(*this, kOsrOffsetOffset, offset.ToInt());
}

bool Code::uses_safepoint_table() const {
  return is_turbofanned() || is_maglevved() || is_wasm_code();
}

int Code::stack_slots() const {
  const int slots = StackSlotsField::decode(flags(kRelaxedLoad));
  DCHECK_IMPLIES(!uses_safepoint_table(), slots == 0);
  return slots;
}

bool Code::marked_for_deoptimization() const {
  return MarkedForDeoptimizationField::decode(flags(kRelaxedLoad));
}

void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(
                           GetIsolateFromWritableObject(*this)));
  int32_t previous = flags(kRelaxedLoad);
  int32_t updated = MarkedForDeoptimizationField::update(previous, flag);
  set_flags(updated, kRelaxedStore);
}

bool Code::embedded_objects_cleared() const {
  return Code::EmbeddedObjectsClearedField::decode(flags(kRelaxedLoad));
}

void Code::set_embedded_objects_cleared(bool flag) {
  DCHECK_IMPLIES(flag, marked_for_deoptimization());
  int32_t previous = flags(kRelaxedLoad);
  int32_t updated = Code::EmbeddedObjectsClearedField::update(previous, flag);
  set_flags(updated, kRelaxedStore);
}

inline bool Code::can_have_weak_objects() const {
  return CanHaveWeakObjectsField::decode(flags(kRelaxedLoad));
}

inline void Code::set_can_have_weak_objects(bool value) {
  int32_t previous = flags(kRelaxedLoad);
  int32_t updated = CanHaveWeakObjectsField::update(previous, value);
  set_flags(updated, kRelaxedStore);
}

bool Code::is_wasm_code() const { return kind() == CodeKind::WASM_FUNCTION; }

int Code::constant_pool_offset() const {
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    // Redirection needed since the field doesn't exist in this case.
    return code_comments_offset();
  }
  return ReadField<int>(kConstantPoolOffsetOffset);
}

void Code::set_constant_pool_offset(int value) {
  if (!V8_EMBEDDED_CONSTANT_POOL_BOOL) {
    // Redirection needed since the field doesn't exist in this case.
    return;
  }
  DCHECK_LE(value, metadata_size());
  WriteField<int>(kConstantPoolOffsetOffset, value);
}

Address Code::constant_pool() const {
  if (!has_constant_pool()) return kNullAddress;
  return metadata_start() + constant_pool_offset();
}

Address Code::constant_pool(InstructionStream instruction_stream) const {
  if (!has_constant_pool()) return kNullAddress;
  static_assert(InstructionStream::kOnHeapBodyIsContiguous);
  return instruction_stream.instruction_start() + instruction_size() +
         constant_pool_offset();
}

Address Code::code_comments() const {
  return metadata_start() + code_comments_offset();
}

int Code::code_comments_size() const {
  return unwinding_info_offset() - code_comments_offset();
}

bool Code::has_code_comments() const { return code_comments_size() > 0; }

Address Code::unwinding_info_start() const {
  return metadata_start() + unwinding_info_offset();
}

Address Code::unwinding_info_end() const { return metadata_end(); }

int Code::unwinding_info_size() const {
  return static_cast<int>(unwinding_info_end() - unwinding_info_start());
}

bool Code::has_unwinding_info() const { return unwinding_info_size() > 0; }

// static
Code Code::FromTargetAddress(Address address) {
  return InstructionStream::FromTargetAddress(address).code(kAcquireLoad);
}

bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}

bool Code::IsWeakObject(HeapObject object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}

bool Code::IsWeakObjectInOptimizedCode(HeapObject object) {
  Map map_object = object.map(kAcquireLoad);
  if (InstanceTypeChecker::IsMap(map_object)) {
    return Map::cast(object).CanTransition();
  }
  return InstanceTypeChecker::IsPropertyCell(map_object) ||
         InstanceTypeChecker::IsJSReceiver(map_object) ||
         InstanceTypeChecker::IsContext(map_object);
}

bool Code::IsWeakObjectInDeoptimizationLiteralArray(Object object) {
  // Maps must be strong because they can be used as part of the description for
  // how to materialize an object upon deoptimization, in which case it is
  // possible to reach the code that requires the Map without anything else
  // holding a strong pointer to that Map.
  return object.IsHeapObject() && !object.IsMap() &&
         Code::IsWeakObjectInOptimizedCode(HeapObject::cast(object));
}

void Code::IterateDeoptimizationLiterals(RootVisitor* v) {
  if (kind() == CodeKind::BASELINE) return;

  auto deopt_data = DeoptimizationData::cast(deoptimization_data());
  if (deopt_data.length() == 0) return;

  DeoptimizationLiteralArray literals = deopt_data.LiteralArray();
  const int literals_length = literals.length();
  for (int i = 0; i < literals_length; ++i) {
    MaybeObject maybe_literal = literals.Get(i);
    HeapObject heap_literal;
    if (maybe_literal.GetHeapObject(&heap_literal)) {
      v->VisitRootPointer(Root::kStackRoots, "deoptimization literal",
                          FullObjectSlot(&heap_literal));
    }
  }
}

Object Code::raw_instruction_stream() const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::raw_instruction_stream(cage_base);
}

Object Code::raw_instruction_stream(PtrComprCageBase cage_base) const {
  return ExternalCodeField<Object>::load(cage_base, *this);
}

void Code::set_raw_instruction_stream(Object value, WriteBarrierMode mode) {
  ExternalCodeField<Object>::Release_Store(*this, value);
  CONDITIONAL_WRITE_BARRIER(*this, kInstructionStreamOffset, value, mode);
}

bool Code::has_instruction_stream() const {
  const uint32_t value = ReadField<uint32_t>(kInstructionStreamOffset);
  SLOW_DCHECK(value == 0 || !InReadOnlySpace());
  return value != 0;
}

bool Code::has_instruction_stream(RelaxedLoadTag tag) const {
  const uint32_t value =
      RELAXED_READ_INT32_FIELD(*this, kInstructionStreamOffset);
  SLOW_DCHECK(value == 0 || !InReadOnlySpace());
  return value != 0;
}

PtrComprCageBase Code::code_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  return PtrComprCageBase(ExternalCodeCompressionScheme::base());
#else   // V8_EXTERNAL_CODE_SPACE
  // Without external code space: `code_cage_base == main_cage_base`. We can
  // get the main cage base from any heap object, including objects in RO
  // space.
  return GetPtrComprCageBase(*this);
#endif  // V8_EXTERNAL_CODE_SPACE
}

InstructionStream Code::instruction_stream() const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::instruction_stream(cage_base);
}

InstructionStream Code::unchecked_instruction_stream() const {
  return InstructionStream::unchecked_cast(raw_instruction_stream());
}

InstructionStream Code::instruction_stream(PtrComprCageBase cage_base) const {
  DCHECK(has_instruction_stream());
  return ExternalCodeField<InstructionStream>::load(cage_base, *this);
}

InstructionStream Code::instruction_stream(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::instruction_stream(cage_base, tag);
}

InstructionStream Code::instruction_stream(PtrComprCageBase cage_base,
                                           RelaxedLoadTag tag) const {
  DCHECK(has_instruction_stream());
  return ExternalCodeField<InstructionStream>::Relaxed_Load(cage_base, *this);
}

Object Code::raw_instruction_stream(RelaxedLoadTag tag) const {
  PtrComprCageBase cage_base = code_cage_base();
  return Code::raw_instruction_stream(cage_base, tag);
}

Object Code::raw_instruction_stream(PtrComprCageBase cage_base,
                                    RelaxedLoadTag tag) const {
  return ExternalCodeField<Object>::Relaxed_Load(cage_base, *this);
}

DEF_GETTER(Code, instruction_start, Address) {
  return ReadField<Address>(kInstructionStartOffset);
}

void Code::init_instruction_start(Isolate* isolate, Address value) {
  set_instruction_start(isolate, value);
}

void Code::set_instruction_start(Isolate* isolate, Address value) {
  WriteField<Address>(kInstructionStartOffset, value);
}

void Code::SetInstructionStreamAndInstructionStart(Isolate* isolate_for_sandbox,
                                                   InstructionStream code,
                                                   WriteBarrierMode mode) {
  set_raw_instruction_stream(code, mode);
  set_instruction_start(isolate_for_sandbox, code.instruction_start());
}

void Code::SetInstructionStartForOffHeapBuiltin(Isolate* isolate_for_sandbox,
                                                Address entry) {
  DCHECK(!has_instruction_stream());
  set_instruction_start(isolate_for_sandbox, entry);
}

void Code::SetInstructionStartForSerialization(Isolate* isolate,
                                               Address entry) {
  set_instruction_start(isolate, entry);
}

void Code::UpdateInstructionStart(Isolate* isolate_for_sandbox,
                                  InstructionStream istream) {
  DCHECK_EQ(raw_instruction_stream(), istream);
  set_instruction_start(isolate_for_sandbox, istream.instruction_start());
}

void Code::clear_padding() {
  memset(reinterpret_cast<void*>(address() + kUnalignedSize), 0,
         kSize - kUnalignedSize);
}

RELAXED_UINT32_ACCESSORS(Code, flags, kFlagsOffset)

void Code::initialize_flags(CodeKind kind, bool is_turbofanned,
                            int stack_slots) {
  CHECK(0 <= stack_slots && stack_slots < StackSlotsField::kMax);
  DCHECK(!CodeKindIsInterpretedJSFunction(kind));
  uint32_t value = KindField::encode(kind) |
                   IsTurbofannedField::encode(is_turbofanned) |
                   StackSlotsField::encode(stack_slots);
  static_assert(FIELD_SIZE(kFlagsOffset) == kInt32Size);
  set_flags(value, kRelaxedStore);
  DCHECK_IMPLIES(stack_slots != 0, uses_safepoint_table());
  DCHECK_IMPLIES(!uses_safepoint_table(), stack_slots == 0);
}

// Ensure builtin_id field fits into int16_t, so that we can rely on sign
// extension to convert int16_t{-1} to kNoBuiltinId.
// If the asserts fail, update the code that use kBuiltinIdOffset below.
static_assert(static_cast<int>(Builtin::kNoBuiltinId) == -1);
static_assert(Builtins::kBuiltinCount < std::numeric_limits<int16_t>::max());

void Code::set_builtin_id(Builtin builtin_id) {
  static_assert(FIELD_SIZE(kBuiltinIdOffset) == kInt16Size);
  WriteField<int16_t>(kBuiltinIdOffset, static_cast<int16_t>(builtin_id));
}

Builtin Code::builtin_id() const {
  // Rely on sign-extension when converting int16_t to int to preserve
  // kNoBuiltinId value.
  static_assert(FIELD_SIZE(kBuiltinIdOffset) == kInt16Size);
  static_assert(static_cast<int>(static_cast<int16_t>(Builtin::kNoBuiltinId)) ==
                static_cast<int>(Builtin::kNoBuiltinId));
  int value = ReadField<int16_t>(kBuiltinIdOffset);
  return static_cast<Builtin>(value);
}

bool Code::is_builtin() const { return builtin_id() != Builtin::kNoBuiltinId; }

bool Code::is_optimized_code() const {
  return CodeKindIsOptimizedJSFunction(kind());
}

inline bool Code::is_interpreter_trampoline_builtin() const {
  return IsInterpreterTrampolineBuiltin(builtin_id());
}

inline bool Code::is_baseline_trampoline_builtin() const {
  return IsBaselineTrampolineBuiltin(builtin_id());
}

inline bool Code::is_baseline_leave_frame_builtin() const {
  return builtin_id() == Builtin::kBaselineLeaveFrame;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_INL_H_
