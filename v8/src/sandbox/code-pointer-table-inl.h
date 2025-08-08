// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_TABLE_INL_H_
#define V8_SANDBOX_CODE_POINTER_TABLE_INL_H_

#include "src/sandbox/code-pointer-table.h"
#include "src/sandbox/external-entity-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

void CodePointerTableEntry::MakeCodePointerEntry(Address code,
                                                 Address entrypoint) {
  // The marking bit is the LSB of the code pointer, which should always be set
  // here since it is supposed to be a tagged pointer.
  DCHECK_EQ(code & kMarkingBit, kMarkingBit);
  entrypoint_.store(entrypoint, std::memory_order_relaxed);
  code_.store(code, std::memory_order_relaxed);
}

Address CodePointerTableEntry::GetEntrypoint() const {
  DCHECK(!IsFreelistEntry());
  return entrypoint_.load(std::memory_order_relaxed);
}

void CodePointerTableEntry::SetEntrypoint(Address value) {
  DCHECK(!IsFreelistEntry());
  entrypoint_.store(value, std::memory_order_relaxed);
}

Address CodePointerTableEntry::GetCodeObject() const {
  DCHECK(!IsFreelistEntry());
  // We reuse the heap object tag bit as marking bit, so we need to explicitly
  // set it here when accessing the pointer.
  return code_.load(std::memory_order_relaxed) | kMarkingBit;
}

void CodePointerTableEntry::SetCodeObject(Address value) {
  DCHECK_EQ(value & kMarkingBit, kMarkingBit);
  DCHECK(!IsFreelistEntry());
  code_.store(value, std::memory_order_relaxed);
}

void CodePointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  Address value = kFreeEntryTag | next_entry_index;
  entrypoint_.store(value, std::memory_order_relaxed);
  code_.store(kNullAddress, std::memory_order_relaxed);
}

bool CodePointerTableEntry::IsFreelistEntry() const {
  auto entrypoint = entrypoint_.load(std::memory_order_relaxed);
  return (entrypoint & kFreeEntryTag) == kFreeEntryTag;
}

uint32_t CodePointerTableEntry::GetNextFreelistEntryIndex() const {
  return static_cast<uint32_t>(entrypoint_.load(std::memory_order_relaxed));
}

void CodePointerTableEntry::Mark() {
  Address old_value = code_.load(std::memory_order_relaxed);
  Address new_value = old_value | kMarkingBit;

  // We don't need to perform the CAS in a loop since it can only fail if a new
  // value has been written into the entry. This, however, will also have set
  // the marking bit.
  bool success = code_.compare_exchange_strong(old_value, new_value,
                                               std::memory_order_relaxed);
  DCHECK(success || (old_value & kMarkingBit) == kMarkingBit);
  USE(success);
}

void CodePointerTableEntry::Unmark() {
  Address value = code_.load(std::memory_order_relaxed);
  value &= ~kMarkingBit;
  code_.store(value, std::memory_order_relaxed);
}

bool CodePointerTableEntry::IsMarked() const {
  Address value = code_.load(std::memory_order_relaxed);
  return value & kMarkingBit;
}

Address CodePointerTable::GetEntrypoint(CodePointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetEntrypoint();
}

Address CodePointerTable::GetCodeObject(CodePointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetCodeObject();
}

void CodePointerTable::SetEntrypoint(CodePointerHandle handle, Address value) {
  DCHECK_NE(kNullCodePointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  at(index).SetEntrypoint(value);
}

void CodePointerTable::SetCodeObject(CodePointerHandle handle, Address value) {
  DCHECK_NE(kNullCodePointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  at(index).SetCodeObject(value);
}

CodePointerHandle CodePointerTable::AllocateAndInitializeEntry(
    Space* space, Address code, Address entrypoint) {
  DCHECK(space->BelongsTo(this));
  uint32_t index = AllocateEntry(space);
  at(index).MakeCodePointerEntry(code, entrypoint);
  return IndexToHandle(index);
}

void CodePointerTable::Mark(Space* space, CodePointerHandle handle) {
  DCHECK(space->BelongsTo(this));
  // The null entry is immortal and immutable, so no need to mark it as alive.
  if (handle == kNullCodePointerHandle) return;

  uint32_t index = HandleToIndex(handle);
  DCHECK(space->Contains(index));

  at(index).Mark();
}

uint32_t CodePointerTable::HandleToIndex(CodePointerHandle handle) const {
  uint32_t index = handle >> kCodePointerHandleShift;
  DCHECK_EQ(handle, index << kCodePointerHandleShift);
  return index;
}

CodePointerHandle CodePointerTable::IndexToHandle(uint32_t index) const {
  CodePointerHandle handle = index << kCodePointerHandleShift;
  DCHECK_EQ(index, handle >> kCodePointerHandleShift);
  return handle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_CODE_POINTER_TABLE_INL_H_
