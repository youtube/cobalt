// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_OBJECT_POOL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_OBJECT_POOL_H_

#include "starboard/shared/starboard/player/fixed_size_memory_pool.h"

namespace starboard {

// A thread-safe object pool for recycling memory allocations of a fixed size.
// It wraps FixedSizeMemoryPool and adds fallback to heap allocation when
// the pre-allocated pool is exhausted.
class ObjectPool {
 public:
  // Constructor.
  // |element_size| is the size of each object in bytes.
  // |capacity| is the number of objects to pre-allocate. Must be > 0.
  ObjectPool(size_t element_size, size_t capacity);

  ~ObjectPool() = default;

  // Disallow copy and assign.
  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;

  // Allocates a block of memory of element_size.
  // Returns a recycled block from the pool if available, or allocates a new one
  // from heap.
  void* Allocate();

  // Returns a block of memory back to the pool for future reuse
  // if it belongs to the pool. Otherwise, deletes it.
  void Free(void* ptr);

  // Helper to check if a pointer belongs to the pre-allocated pool.
  bool IsFromPreallocatedPool(void* ptr) const;

  // Getters for testing
  size_t element_size() const;
  size_t capacity() const;
  size_t free_list_size() const;

 private:
  FixedSizeMemoryPool pool_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_OBJECT_POOL_H_
