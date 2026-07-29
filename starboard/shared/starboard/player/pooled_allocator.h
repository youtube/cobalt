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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_POOLED_ALLOCATOR_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_POOLED_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace starboard {

namespace internal {

// A thread-safe memory pool that prioritizes allocating memory blocks of a
// constant size from a single pre-allocated contiguous block to eliminate
// memory fragmentation and maximize performance.
//
// Threading Model: Thread-safe. All public methods are protected by an
// internal mutex.
//
// Expected Lifetime & Ownership: Typically wrapped inside a `PooledAllocator`
// and owned as a global singleton (e.g. using
// `starboard::NoDestructor`) by specific media components (like decoders or
// buffers) to ensure the pre-allocated memory persists for the  entire
// execution lifetime of the app.
//
// This is a "pure" pool: it returns nullptr on exhaustion, and does not handle
// heap fallback itself. Fallback is handled by PooledAllocator.
class FixedBlockPool {
 public:
  struct InfoForTesting {
    size_t block_size;
    size_t total_blocks;
    size_t free_blocks;
  };

  FixedBlockPool(std::string_view name, size_t block_size, size_t total_blocks);
  ~FixedBlockPool();

  // Disallow copy and assign.
  FixedBlockPool(const FixedBlockPool&) = delete;
  FixedBlockPool& operator=(const FixedBlockPool&) = delete;

  // Attempts to allocate a block of memory from the pool.
  // Returns a pointer to the allocated block on success, or nullptr if
  // exhausted.
  void* TryAllocate();

  // Attempts to free a block of memory.
  // If |ptr| belongs to the pool, it is returned to the pool and returns true.
  // Otherwise, returns false.
  bool TryFree(void* ptr);

  size_t block_size() const { return block_size_; }
  size_t total_blocks() const { return total_blocks_; }
  std::string_view name() const { return name_; }

  // For testing
  InfoForTesting GetInfoForTesting() const;

 private:
  size_t GetBlockIndex(const void* ptr) const;

  const std::string name_;
  const size_t block_size_;
  const size_t total_blocks_;
  const std::unique_ptr<uint8_t[]> pool_storage_;

  void MaybeLogStatus_Locked();

  mutable std::mutex mutex_;
  std::vector<void*> free_list_;       // Guarded by |mutex_|.
  int64_t last_status_logged_us_ = 0;  // Guarded by |mutex_|.

  std::vector<bool> is_block_allocated_;  // Guarded by |mutex_|.
};

}  // namespace internal

// PooledAllocator wraps one or more FixedBlockPool instances and adds heap
// fallback logic (::operator new/delete) when the pools are disabled,
// exhausted, or the request is oversized.
// It supports N-tiered pooling with strict best-fit routing.
//
// Threading Model: Thread-safe. It does not modify its own state after
// construction, and delegates allocations/deallocations to thread-safe
// FixedBlockPool instances.
//
// Expected Lifetime & Ownership: Typically owned as a global singleton
// (e.g. using `starboard::NoDestructor` or `starboard::LazyInitializer`) by
// specific media components to ensure the pools persist for the entire
// execution lifetime.
class PooledAllocator {
 public:
  struct PoolConfig {
    std::string name;
    size_t block_size;
    size_t total_blocks;
  };

  // Constructor for multiple pools (ordered automatically by block size).
  explicit PooledAllocator(std::vector<PoolConfig> configs);

  // Helper constructor for a single pool.
  PooledAllocator(std::string_view name,
                  size_t block_size,
                  size_t total_blocks);

  ~PooledAllocator();

  // Disallow copy and assign.
  PooledAllocator(const PooledAllocator&) = delete;
  PooledAllocator& operator=(const PooledAllocator&) = delete;

  // Allocates memory.
  // Attempts to allocate from the managed pools.
  // If all eligible pools are exhausted or the size is too large for all pools,
  // falls back to standard heap allocation (::operator new).
  void* Allocate(size_t size);

  // Frees memory.
  // Attempts to return the memory to one of the managed pools. If none of the
  // pools own the pointer, it is safely freed using standard heap deallocation
  // (::operator delete).
  void Free(void* ptr);

  const std::vector<std::unique_ptr<internal::FixedBlockPool>>&
  pools_for_testing() const {
    return pools_;
  }

 private:
  std::vector<std::unique_ptr<internal::FixedBlockPool>> pools_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_POOLED_ALLOCATOR_H_
