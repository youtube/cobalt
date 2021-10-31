/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef NB_STARBOARD_MEMORY_ALLOCATOR_H_
#define NB_STARBOARD_MEMORY_ALLOCATOR_H_

#include "nb/allocator.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace nb {

// StarboardMemoryAllocator is an allocator that allocates and frees memory
// using SbMemoryAllocateAligned() and SbMemoryDeallocate() in
// starboard/memory.h.
class StarboardMemoryAllocator : public Allocator {
 public:
  void* Allocate(std::size_t size) override { return Allocate(size, 1); }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    return SbMemoryAllocateAligned(alignment, size);
  }

  void* AllocateForAlignment(std::size_t* size,
                             std::size_t alignment) override {
    return Allocate(*size, alignment);
  }
  void Free(void* memory) override { SbMemoryDeallocateAligned(memory); }
  std::size_t GetCapacity() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  std::size_t GetAllocated() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  void PrintAllocations() const override {}
};

}  // namespace nb

#endif  // NB_STARBOARD_MEMORY_ALLOCATOR_H_
