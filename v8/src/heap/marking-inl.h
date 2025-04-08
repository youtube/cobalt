// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_INL_H_
#define V8_HEAP_MARKING_INL_H_

#include "src/base/build_config.h"
#include "src/base/macros.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/heap-inl.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/spaces.h"

namespace v8::internal {

template <>
inline void MarkingBitmap::SetBitsInCell<AccessMode::NON_ATOMIC>(
    uint32_t cell_index, uint32_t mask) {
  cells()[cell_index] |= mask;
}

template <>
inline void MarkingBitmap::SetBitsInCell<AccessMode::ATOMIC>(
    uint32_t cell_index, uint32_t mask) {
  base::AsAtomic32::SetBits(cells() + cell_index, mask, mask);
}

template <>
inline void MarkingBitmap::ClearBitsInCell<AccessMode::NON_ATOMIC>(
    uint32_t cell_index, uint32_t mask) {
  cells()[cell_index] &= ~mask;
}

template <>
inline void MarkingBitmap::ClearBitsInCell<AccessMode::ATOMIC>(
    uint32_t cell_index, uint32_t mask) {
  base::AsAtomic32::SetBits(cells() + cell_index, 0u, mask);
}

template <>
inline void MarkingBitmap::ClearCellRangeRelaxed<AccessMode::ATOMIC>(
    uint32_t start_cell_index, uint32_t end_cell_index) {
  base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
  for (uint32_t i = start_cell_index; i < end_cell_index; i++) {
    base::Relaxed_Store(cell_base + i, 0);
  }
}

template <>
inline void MarkingBitmap::ClearCellRangeRelaxed<AccessMode::NON_ATOMIC>(
    uint32_t start_cell_index, uint32_t end_cell_index) {
  for (uint32_t i = start_cell_index; i < end_cell_index; i++) {
    cells()[i] = 0;
  }
}

template <>
inline void MarkingBitmap::SetCellRangeRelaxed<AccessMode::ATOMIC>(
    uint32_t start_cell_index, uint32_t end_cell_index) {
  base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
  for (uint32_t i = start_cell_index; i < end_cell_index; i++) {
    base::Relaxed_Store(cell_base + i, 0xffffffff);
  }
}

template <>
inline void MarkingBitmap::SetCellRangeRelaxed<AccessMode::NON_ATOMIC>(
    uint32_t start_cell_index, uint32_t end_cell_index) {
  for (uint32_t i = start_cell_index; i < end_cell_index; i++) {
    cells()[i] = 0xffffffff;
  }
}

template <AccessMode mode>
void MarkingBitmap::Clear() {
  ClearCellRangeRelaxed<mode>(0, kCellsCount);
  if constexpr (mode == AccessMode::ATOMIC) {
    // This fence prevents re-ordering of publishing stores with the mark-bit
    // setting stores.
    base::SeqCst_MemoryFence();
  }
}

template <AccessMode mode>
inline void MarkingBitmap::SetRange(MarkBitIndex start_index,
                                    MarkBitIndex end_index) {
  if (start_index >= end_index) return;
  end_index--;

  const CellIndex start_cell_index = IndexToCell(start_index);
  const MarkBit::CellType start_index_mask = IndexInCellMask(start_index);
  const CellIndex end_cell_index = IndexToCell(end_index);
  const MarkBit::CellType end_index_mask = IndexInCellMask(end_index);

  if (start_cell_index != end_cell_index) {
    // Firstly, fill all bits from the start address to the end of the first
    // cell with 1s.
    SetBitsInCell<mode>(start_cell_index, ~(start_index_mask - 1));
    // Then fill all in between cells with 1s.
    SetCellRangeRelaxed<mode>(start_cell_index + 1, end_cell_index);
    // Finally, fill all bits until the end address in the last cell with 1s.
    SetBitsInCell<mode>(end_cell_index, end_index_mask | (end_index_mask - 1));
  } else {
    SetBitsInCell<mode>(start_cell_index,
                        end_index_mask | (end_index_mask - start_index_mask));
  }
  if (mode == AccessMode::ATOMIC) {
    // This fence prevents re-ordering of publishing stores with the mark-bit
    // setting stores.
    base::SeqCst_MemoryFence();
  }
}

template <AccessMode mode>
inline void MarkingBitmap::ClearRange(MarkBitIndex start_index,
                                      MarkBitIndex end_index) {
  if (start_index >= end_index) return;
  end_index--;

  const CellIndex start_cell_index = IndexToCell(start_index);
  const MarkBit::CellType start_index_mask = IndexInCellMask(start_index);
  const CellIndex end_cell_index = IndexToCell(end_index);
  const MarkBit::CellType end_index_mask = IndexInCellMask(end_index);

  if (start_cell_index != end_cell_index) {
    // Firstly, fill all bits from the start address to the end of the first
    // cell with 0s.
    ClearBitsInCell<mode>(start_cell_index, ~(start_index_mask - 1));
    // Then fill all in between cells with 0s.
    ClearCellRangeRelaxed<mode>(start_cell_index + 1, end_cell_index);
    // Finally, set all bits until the end address in the last cell with 0s.
    ClearBitsInCell<mode>(end_cell_index,
                          end_index_mask | (end_index_mask - 1));
  } else {
    ClearBitsInCell<mode>(start_cell_index,
                          end_index_mask | (end_index_mask - start_index_mask));
  }
  if (mode == AccessMode::ATOMIC) {
    // This fence prevents re-ordering of publishing stores with the mark-bit
    // clearing stores.
    base::SeqCst_MemoryFence();
  }
}

// static
MarkingBitmap* MarkingBitmap::FromAddress(Address address) {
  Address page_address = address & ~kPageAlignmentMask;
  return Cast(page_address + MemoryChunkLayout::kMarkingBitmapOffset);
}

// static
MarkBit MarkingBitmap::MarkBitFromAddress(Address address) {
  const auto index = AddressToIndex(address);
  const auto mask = IndexInCellMask(index);
  MarkBit::CellType* cell = FromAddress(address)->cells() + IndexToCell(index);
  return MarkBit(cell, mask);
}

// static
constexpr MarkingBitmap::MarkBitIndex MarkingBitmap::LimitAddressToIndex(
    Address address) {
  if (IsAligned(address, BasicMemoryChunk::kAlignment)) return kLength;
  return AddressToIndex(address);
}

// static
MarkBit MarkBit::From(Address address) {
  return MarkingBitmap::MarkBitFromAddress(address);
}

// static
MarkBit MarkBit::From(HeapObject heap_object) {
  return MarkingBitmap::MarkBitFromAddress(heap_object.ptr());
}

LiveObjectRange::iterator::iterator() : cage_base_(kNullAddress) {}

LiveObjectRange::iterator::iterator(const Page* page)
    : page_(page),
      cells_(page->marking_bitmap()->cells()),
      cage_base_(page->heap()->isolate()),
      current_cell_index_(MarkingBitmap::IndexToCell(
          MarkingBitmap::AddressToIndex(page->area_start()))),
      current_cell_(cells_[current_cell_index_]) {
  AdvanceToNextValidObject();
}

LiveObjectRange::iterator& LiveObjectRange::iterator::operator++() {
  AdvanceToNextValidObject();
  return *this;
}

LiveObjectRange::iterator LiveObjectRange::iterator::operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

void LiveObjectRange::iterator::AdvanceToNextValidObject() {
  // If we found a regular object we are done. In case of free space, we
  // need to continue.
  //
  // Reading the instance type of the map is safe here even in the presence
  // of the mutator writing a new Map because Map objects are published with
  // release stores (or are otherwise read-only) and the map is retrieved  in
  // `AdvanceToNextMarkedObject()` using an acquire load.
  while (AdvanceToNextMarkedObject() &&
         InstanceTypeChecker::IsFreeSpaceOrFiller(current_map_)) {
  }
}

bool LiveObjectRange::iterator::AdvanceToNextMarkedObject() {
  // The following block moves the iterator to the next cell from the current
  // object. This means skipping all possibly set mark bits (in case of black
  // allocation).
  if (!current_object_.is_null()) {
    // Compute an end address that is inclusive. This allows clearing the cell
    // up and including the end address. This works for one word fillers as
    // well as other objects.
    Address next_object = current_object_.address() + current_size_;
    current_object_ = HeapObject();
    if (IsAligned(next_object, BasicMemoryChunk::kAlignment)) {
      return false;
    }
    // Area end may not be exactly aligned to kAlignment. We don't need to bail
    // out for area_end() though as we are guaranteed to have a bit for the
    // whole page.
    DCHECK_LE(next_object, page_->area_end());
    // Move to the corresponding cell of the end index.
    const auto next_markbit_index = MarkingBitmap::AddressToIndex(next_object);
    DCHECK_GE(MarkingBitmap::IndexToCell(next_markbit_index),
              current_cell_index_);
    current_cell_index_ = MarkingBitmap::IndexToCell(next_markbit_index);
    DCHECK_LT(current_cell_index_, MarkingBitmap::kCellsCount);
    // Mask out lower addresses in the cell.
    const MarkBit::CellType mask =
        MarkingBitmap::IndexInCellMask(next_markbit_index);
    current_cell_ = cells_[current_cell_index_] & ~(mask - 1);
  }
  // The next block finds any marked object starting from the current cell.
  while (true) {
    if (current_cell_) {
      const auto trailing_zeros = base::bits::CountTrailingZeros(current_cell_);
      Address current_cell_base =
          page_->address() + MarkingBitmap::CellToBase(current_cell_index_);
      Address object_address = current_cell_base + trailing_zeros * kTaggedSize;
      // The object may be a filler which we want to skip.
      current_object_ = HeapObject::FromAddress(object_address);
      current_map_ = current_object_.map(cage_base_, kAcquireLoad);
      DCHECK(MapWord::IsMapOrForwarded(current_map_));
      current_size_ = ALIGN_TO_ALLOCATION_ALIGNMENT(
          current_object_.SizeFromMap(current_map_));
      CHECK(page_->ContainsLimit(object_address + current_size_));
      return true;
    }
    if (++current_cell_index_ >= MarkingBitmap::kCellsCount) break;
    current_cell_ = cells_[current_cell_index_];
  }
  return false;
}

LiveObjectRange::iterator LiveObjectRange::begin() { return iterator(page_); }

LiveObjectRange::iterator LiveObjectRange::end() { return iterator(); }

}  // namespace v8::internal

#endif  // V8_HEAP_MARKING_INL_H_
