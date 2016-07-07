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

#ifndef NB_REUSE_ALLOCATOR_H_
#define NB_REUSE_ALLOCATOR_H_

#include <list>
#include <map>
#include <set>
#include <vector>

#include "nb/allocator.h"
#include "starboard/types.h"

namespace nb {

// An allocator designed to accomodate cases where the memory allocated may not
// be efficient or safe to access via the CPU.  It solves this problem by
// maintaining all allocation meta data is outside of the allocated memory.
// It is passed a fallback allocator that it can request additional memory
// from as needed.
class ReuseAllocator : public Allocator {
 public:
  explicit ReuseAllocator(Allocator* fallback_allocator);
  virtual ~ReuseAllocator();

  // Search free memory blocks for an existing one, and if none are large
  // enough, allocate a new one from no-free memory and return that.
  void* Allocate(size_t size);
  void* Allocate(size_t size, size_t alignment);
  void* AllocateForAlignment(size_t size, size_t alignment);

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory);

  size_t GetCapacity() const { return capacity_; }
  size_t GetAllocated() const { return total_allocated_; }

  void PrintAllocations() const;

 private:
  // We will allocate from the given allocator whenever we can't find
  // pre-used memory to allocate.
  Allocator* fallback_allocator_;

  struct MemoryBlock {
    uintptr_t address;
    size_t size;
    bool operator<(const MemoryBlock& other) const {
      return address < other.address;
    }
  };
  // Freelist sorted by address.
  typedef std::set<MemoryBlock> FreeBlockSet;
  // Map from pointers we returned to the user, back to memory blocks.
  typedef std::map<uintptr_t, MemoryBlock> AllocatedBlockMap;
  void AddFreeBlock(uintptr_t address, size_t size);
  void RemoveFreeBlock(FreeBlockSet::iterator);

  FreeBlockSet free_blocks_;
  AllocatedBlockMap allocated_blocks_;

  // A list of allocations made from the fallback allocator.  We keep track
  // of this so that we can free them all upon our destruction.
  std::vector<void*> fallback_allocations_;

  // How much we have allocated from the fallback allocator.
  size_t capacity_;

  // How much has been allocated from us.
  size_t total_allocated_;
};

}  // namespace nb

#endif  // NB_REUSE_ALLOCATOR_H_
