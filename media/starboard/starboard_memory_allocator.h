// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_MEDIA_STARBOARD_MEMORY_ALLOCATOR_H_
#define COBALT_MEDIA_STARBOARD_MEMORY_ALLOCATOR_H_

#include <stdlib.h>

#include <algorithm>

#include "starboard/common/allocator.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace media {

// StarboardMemoryAllocator is an allocator that allocates and frees memory
// using posix_memalign() and free().
class StarboardMemoryAllocator : public starboard::common::Allocator {
 public:
  void* Allocate(std::size_t size) override { return Allocate(size, 1); }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    void* p = nullptr;
    posix_memalign(&p, std::max(alignment, sizeof(void*)), size);
    return p;
  }

  void* AllocateForAlignment(std::size_t* size,
                             std::size_t alignment) override {
    return Allocate(*size, alignment);
  }
  void Free(void* memory) override { free(memory); }
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

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_STARBOARD_MEMORY_ALLOCATOR_H_
