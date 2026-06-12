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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_BLOCK_POOL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_BLOCK_POOL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace starboard {

// A thread-safe memory pool that prioritizes allocating memory blocks of a
// constant size from a single pre-allocated contiguous block to eliminate
// memory fragmentation and maximize performance.
//
// To ensure reliability under heavy load or varied allocation sizes, this pool
// automatically falls back to standard heap allocation (using ::operator new)
// if the pool is exhausted, or if the requested allocation size exceeds the
// pool's configured block size.
//
// Thread Safety:
// This class is fully thread-safe. All public methods (Allocate, Free, etc.)
// are protected by an internal mutex, allowing safe concurrent access from
// multiple threads.
//
// Lifetime and Ownership:
// This class does not manage its own lifetime. It is intended to be owned
// by its creator—either as a long-lived global singleton (e.g., via
// LazyInitializer) for shared subsystem-level pools, or tied to the lifetime
// of specific player components (e.g., as a unique_ptr member). Users must
// ensure the pool outlives all allocations made from it, as freeing a block
// after the pool is destroyed is undefined behavior.
class FixedBlockPool {
 public:
  // Constructor.
  // |name| is the name of the pool for logging.
  // |block_size| is the size of each block in bytes.
  // |total_blocks| is the number of blocks to pre-allocate. Must be > 0.
  FixedBlockPool(std::string_view name, size_t block_size, size_t total_blocks);
  ~FixedBlockPool();

  // Disallow copy and assign.
  FixedBlockPool(const FixedBlockPool&) = delete;
  FixedBlockPool& operator=(const FixedBlockPool&) = delete;

  // Allocates a block of memory.
  // If |size| is <= block_size() and the pool has free blocks, allocates from
  // the pool. Otherwise, falls back to heap allocation (using ::operator new).
  void* Allocate(size_t size);

  // Frees a block of memory.
  // If |ptr| belongs to the pool, it is returned to the pool.
  // Otherwise, it is freed from the heap (using ::operator delete).
  void Free(void* ptr);

  bool IsFromPool(const void* ptr) const;

  struct InfoForTesting {
    size_t block_size;
    size_t total_blocks;
    size_t free_blocks;
  };
  InfoForTesting GetInfoForTesting() const;

  std::string_view name() const { return name_; }

 private:
  void AssertValidPoolPtr(const void* ptr) const;

  size_t GetBlockIndex(const void* ptr) const;

  const std::string name_;
  const size_t block_size_;
  const size_t total_blocks_;
  const std::unique_ptr<uint8_t[]> pool_storage_;

  void MaybeLogStatus();

  mutable std::mutex mutex_;
  std::vector<void*> free_list_;       // Guarded by |mutex_|.
  int64_t last_status_logged_us_ = 0;  // Guarded by |mutex_|.

  std::vector<bool> is_block_allocated_;  // Guarded by |mutex_|.
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FIXED_BLOCK_POOL_H_
