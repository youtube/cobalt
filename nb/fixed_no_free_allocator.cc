/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/fixed_no_free_allocator.h"

#include <algorithm>

#include "nb/pointer_arithmetic.h"
#include "starboard/log.h"

namespace nb {

FixedNoFreeAllocator::FixedNoFreeAllocator(void* memory_start,
                                           std::size_t memory_size)
    : memory_start_(memory_start),
      memory_end_(AsPointer(AsInteger(memory_start) + memory_size)) {
  next_memory_ = memory_start_;
}

void FixedNoFreeAllocator::Free(void* memory) {
  // Nothing to do here besides ensure that the freed memory belongs to us.
  if (memory < memory_start_ || memory >= memory_end_) {
    SB_NOTREACHED() << "Invalid block to free: |memory| is " << memory
                    << ", start is " << memory_start_ << ", and end is "
                    << memory_end_;
  }
}

std::size_t FixedNoFreeAllocator::GetCapacity() const {
  return AsInteger(memory_end_) - AsInteger(memory_start_);
}

std::size_t FixedNoFreeAllocator::GetAllocated() const {
  return AsInteger(next_memory_) - AsInteger(memory_start_);
}

void FixedNoFreeAllocator::PrintAllocations() const {
  SB_NOTIMPLEMENTED();
}

void* FixedNoFreeAllocator::Allocate(std::size_t* size,
                                     std::size_t alignment,
                                     bool align_pointer) {
  *size = std::max<std::size_t>(*size, 1);

  // Find the next aligned memory available.
  uint8_t* aligned_next_memory =
      AsPointer(AlignUp(AsInteger(next_memory_), alignment));

  if (aligned_next_memory + *size < aligned_next_memory) {
    // "aligned_next_memory + size" overflows.
    return NULL;
  }

  if (aligned_next_memory + *size > memory_end_) {
    // We don't have enough memory available to make this allocation.
    return NULL;
  }

  if (!align_pointer) {
    *size += AsInteger(aligned_next_memory) - AsInteger(next_memory_);
  }

  void* memory_pointer = align_pointer ? aligned_next_memory : next_memory_;
  next_memory_ = aligned_next_memory + *size;
  return memory_pointer;
}

}  // namespace nb
