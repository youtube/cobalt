// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
#define V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_

#include "src/sandbox/external-entity-table-inl.h"
#include "src/sandbox/trusted-pointer-table.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

void TrustedPointerTableEntry::MakeTrustedPointerEntry(Address content) {
  // The marking bit is the LSB of the pointer, which should always be set here
  // since it is supposed to be a tagged pointer.
  DCHECK_EQ(content & kMarkingBit, kMarkingBit);
  content_.store(content, std::memory_order_relaxed);
}

void TrustedPointerTableEntry::MakeFreelistEntry(uint32_t next_entry_index) {
  Address content = kFreeEntryTag | next_entry_index;
  content_.store(content, std::memory_order_relaxed);
}

Address TrustedPointerTableEntry::GetContent() const {
  DCHECK(!IsFreelistEntry());
  return content_.load(std::memory_order_relaxed);
}

void TrustedPointerTableEntry::SetContent(Address content) {
  DCHECK(!IsFreelistEntry());
  content_.store(content, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsFreelistEntry() const {
  auto content = content_.load(std::memory_order_relaxed);
  return (content & kFreeEntryTag) == kFreeEntryTag;
}

uint32_t TrustedPointerTableEntry::GetNextFreelistEntryIndex() const {
  return static_cast<uint32_t>(content_.load(std::memory_order_relaxed));
}

void TrustedPointerTableEntry::Mark() {
  Address old_value = content_.load(std::memory_order_relaxed);
  Address new_value = old_value | kMarkingBit;

  // We don't need to perform the CAS in a loop since it can only fail if a new
  // value has been written into the entry. This, however, will also have set
  // the marking bit.
  bool success = content_.compare_exchange_strong(old_value, new_value,
                                                  std::memory_order_relaxed);
  DCHECK(success || (old_value & kMarkingBit) == kMarkingBit);
  USE(success);
}

void TrustedPointerTableEntry::Unmark() {
  Address content = content_.load(std::memory_order_relaxed);
  content &= ~kMarkingBit;
  content_.store(content, std::memory_order_relaxed);
}

bool TrustedPointerTableEntry::IsMarked() const {
  Address value = content_.load(std::memory_order_relaxed);
  return value & kMarkingBit;
}

Address TrustedPointerTable::Get(TrustedPointerHandle handle) const {
  uint32_t index = HandleToIndex(handle);
  return at(index).GetContent();
}

void TrustedPointerTable::Set(TrustedPointerHandle handle, Address content) {
  DCHECK_NE(kNullTrustedPointerHandle, handle);
  uint32_t index = HandleToIndex(handle);
  at(index).SetContent(content);
}

TrustedPointerHandle TrustedPointerTable::AllocateAndInitializeEntry(
    Space* space, Address content) {
  DCHECK(space->BelongsTo(this));
  uint32_t index = AllocateEntry(space);
  at(index).MakeTrustedPointerEntry(content);
  return IndexToHandle(index);
}

void TrustedPointerTable::Mark(Space* space, TrustedPointerHandle handle) {
  DCHECK(space->BelongsTo(this));
  // The null entry is immortal and immutable, so no need to mark it as alive.
  if (handle == kNullTrustedPointerHandle) return;

  uint32_t index = HandleToIndex(handle);
  DCHECK(space->Contains(index));

  at(index).Mark();
}

uint32_t TrustedPointerTable::HandleToIndex(TrustedPointerHandle handle) const {
  uint32_t index = handle >> kTrustedPointerHandleShift;
  DCHECK_EQ(handle, index << kTrustedPointerHandleShift);
  return index;
}

TrustedPointerHandle TrustedPointerTable::IndexToHandle(uint32_t index) const {
  TrustedPointerHandle handle = index << kTrustedPointerHandleShift;
  DCHECK_EQ(index, handle >> kTrustedPointerHandleShift);
  return handle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_TRUSTED_POINTER_TABLE_INL_H_
