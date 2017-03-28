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

#ifndef NB_MEMORY_POOL_H_
#define NB_MEMORY_POOL_H_

#include "nb/allocator.h"
#include "nb/fixed_no_free_allocator.h"
#include "nb/reuse_allocator.h"

namespace nb {

// The MemoryPool class can be used to wrap a range of memory with allocators
// such that the memory can be allocated out of and free'd memory re-used
// as necessary.
class MemoryPool : public Allocator {
 public:
  MemoryPool(void* buffer,
             std::size_t size,
             bool verify_full_capacity = false,
             std::size_t small_allocation_threshold = 0);

  void* Allocate(std::size_t size) { return reuse_allocator_.Allocate(size); }
  void* Allocate(std::size_t size, std::size_t alignment) {
    return reuse_allocator_.Allocate(size, alignment);
  }
  void Free(void* memory) { reuse_allocator_.Free(memory); }
  std::size_t GetCapacity() const { return reuse_allocator_.GetCapacity(); }
  std::size_t GetAllocated() const { return reuse_allocator_.GetAllocated(); }

  std::size_t GetHighWaterMark() const {
    return no_free_allocator_.GetAllocated();
  }

  void PrintAllocations() const;

 private:
  // A budget of memory to be used by the memory pool.
  FixedNoFreeAllocator no_free_allocator_;

  // A reuse allocator that will be setup to fallback on the no free allocator
  // to expand its pool as memory is required for which there is no re-usable
  // space already.
  ReuseAllocator reuse_allocator_;
};

}  // namespace nb

#endif  // NB_MEMORY_POOL_H_
