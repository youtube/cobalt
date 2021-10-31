/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef NB_FIXED_NO_FREE_ALLOCATOR_H_
#define NB_FIXED_NO_FREE_ALLOCATOR_H_

#include "nb/allocator.h"

namespace nb {

// FixedNoFreeAllocator is an allocator that allocates memory but cannot reuse
// previously allocated memory.  Specifying that the allocator will not reuse
// previously allocated memory allows for a simple and fast implementation
// of Allocate(), namely that it can continually increment an internal "next
// memory address" pointer.  The allocator does not own the memory it is
// managing.  This is so that logic to deal with how to allocate and free
// the memory range can remain separate from the allocator managing it.
//
// An example usage of this class is to act as a "fallback" allocator to a more
// complicated allocator that can reuse old memory.  When the reuse allocator
// runs out of memory to recycle, it can fall back to this fallback allocator.
// This makes sense if there's a finite area of memory dedicated to graphics
// memory and we would like to wrap it in an allocator.
class FixedNoFreeAllocator : public Allocator {
 public:
  // Requires aligned memory to at least |nb::kMinAlignment|.
  FixedNoFreeAllocator(void* memory_start, std::size_t memory_size);
  void* Allocate(std::size_t size) { return Allocate(&size, 1, true); }

  void* Allocate(std::size_t size, std::size_t alignment) {
    return Allocate(&size, alignment, true);
  }

  void* AllocateForAlignment(std::size_t* size, std::size_t alignment) {
    return Allocate(size, alignment, false);
  }
  void Free(void* memory);
  std::size_t GetCapacity() const;
  std::size_t GetAllocated() const;
  void PrintAllocations() const;

 private:
  void* Allocate(std::size_t* size, std::size_t alignment, bool align_pointer);

  // The start of our memory range, as passed in to the constructor.
  void* const memory_start_;

  // Equal to the first value of memory outside of the allocator's memory range.
  const void* memory_end_;

  // The memory location that will be passed out upon the next allocation.
  void* next_memory_;
};

}  // namespace nb

#endif  // NB_FIXED_NO_FREE_ALLOCATOR_H_
