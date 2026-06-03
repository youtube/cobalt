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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_SIZE_MEMORY_POOL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_SIZE_MEMORY_POOL_H_

#include <cstddef>
#include <mutex>
#include <vector>

namespace starboard {

// A thread-safe, fixed-size memory pool that allocates memory blocks of a
// constant size from a single pre-allocated contiguous block.
// This pool is designed to eliminate memory fragmentation.
// It does NOT handle fallback to heap internally; Allocate() returns nullptr
// if the pool is exhausted.
class FixedSizeMemoryPool {
 public:
  // Constructor.
  // |block_size| is the size of each block in bytes.
  // |capacity| is the number of blocks to pre-allocate. Must be > 0.
  FixedSizeMemoryPool(size_t block_size, size_t capacity);
  ~FixedSizeMemoryPool();

  // Disallow copy and assign.
  FixedSizeMemoryPool(const FixedSizeMemoryPool&) = delete;
  FixedSizeMemoryPool& operator=(const FixedSizeMemoryPool&) = delete;

  // Allocates a block of memory of block_size.
  // Returns a pointer to the allocated block, or nullptr if the pool is
  // exhausted.
  void* Allocate();

  // Returns a block of memory back to the pool.
  // |ptr| must have been allocated from this pool and not yet freed.
  void Free(void* ptr);

  // Helper to check if a pointer belongs to this pool.
  bool IsFromPool(void* ptr) const;

  size_t block_size() const { return block_size_; }
  size_t capacity() const { return capacity_; }
  size_t free_list_size() const;

 private:
  const size_t block_size_;
  const size_t capacity_;
  void* const pool_storage_;

  mutable std::mutex mutex_;
  std::vector<void*> free_list_;  // Guarded by |mutex_|.
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_SIZE_MEMORY_POOL_H_
