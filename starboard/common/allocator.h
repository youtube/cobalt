/*
 * Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_ALLOCATOR_H_
#define STARBOARD_COMMON_ALLOCATOR_H_

#include <cstddef>

namespace starboard {

// The Allocator interface offers a standard and consistent way of allocating
// and freeing memory.  The interface makes no assumption on how the memory
// is allocated and/or freed, and no assumptions about any tracking that may
// occur on the allocations as well.  This must all be defined and implemented
// through derived classes.
class Allocator {
 public:
  // Using a minimum value for alignment keeps things rounded and aligned
  // and help us avoid creating tiny and/or badly misaligned free blocks.  Also
  // ensures even for a 0-byte request will get a unique block.
  static const size_t kMinAlignment;

  virtual ~Allocator() {}

  // TODO: b/369245553 - Cobalt: Consider controlling this via a command line
  // parameter.
  static constexpr int ExtraLogLevel() {
    // 0 => keep allocator related logging to minimum.
    // 1 => enable extra logging for statistics in this class and its users.
    // 2 => enable per allocation logging (extremely chatty).
    return 0;
  }

  // Allocates a range of memory of the given size, without any alignment
  // constraints.
  // Will return NULL if the allocation fails.
  virtual void* Allocate(size_t size) = 0;

  // Allocates a range of memory of the given size with the given alignment.
  // Will return NULL if the allocation fails.
  virtual void* Allocate(size_t size, size_t alignment) = 0;

  // When supported, allocates a range of memory of the given size for the given
  // alignment. Returns a pointer that may not be aligned but points to a memory
  // area that is still large enough when the pointer is subsequently aligned to
  // the given alignment. This allows a reuse allocator to use the padding area
  // as a free block. |size| will be set to the actual size allocated on
  // successful allocations. Will return NULL if the allocation fails. In the
  // case that the underlying block size is inconvenient or impossible to be
  // retrieved, the |size| can remain unchanged for a successful allocation. The
  // user may lose the ability to combine two adjacent allocations in this case.
  // Note that the coding style recommends that in/out parameters to be placed
  // after input parameters but |size| is kept in the left for consistency.
  virtual void* AllocateForAlignment(size_t* /*size*/, size_t /*alignment*/) {
    return 0;
  }

  // Frees memory previously allocated via any call to Allocate().
  virtual void Free(void* memory) = 0;

  // Frees memory with a size. By default it will delegate to Free().
  virtual void FreeWithSize(void* memory, size_t /*size*/) { Free(memory); }

  // Returns the allocator's total capacity for allocations.  It will always
  // be true that GetSize() <= GetCapacity(), though it is possible for
  // capacity to grow and change over time.  It is also possible that due to,
  // say, fragmentation, an allocation may fail even if GetCapacity() reports
  // that enough space is available.
  virtual size_t GetCapacity() const = 0;

  // Returns the allocator's total memory currently allocated.
  virtual size_t GetAllocated() const = 0;

  // Print information for all allocations.
  //
  // When `align_allocated_size` is set to true, the allocated size of
  // individual allocations will be aligned up to the next power of 2 to group
  // more allocations of similar sizes into the same line.
  // `max_allocations_to_print` limits the max lines of allocations to print
  // inside PrintAllocations().
  virtual void PrintAllocations(bool align_allocated_size,
                                int max_allocations_to_print) const = 0;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_ALLOCATOR_H_
