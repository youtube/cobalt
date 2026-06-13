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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration.h"

namespace starboard {

// A thread-safe memory pool that prioritizes allocating memory blocks of a
// constant size from a single pre-allocated contiguous block to eliminate
// memory fragmentation and maximize performance.
//
// This is a "pure" pool: it returns nullptr on exhaustion or oversized
// requests, and does not handle heap fallback itself. Fallback is handled by
// PooledAllocator.
class FixedBlockPool {
 public:
  struct BlockConfig {
    size_t block_size;
    size_t total_blocks;
  };

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

  // Allocates a block of memory.
  // If |size| is <= block_size() and the pool has free blocks, allocates from
  // the pool. Otherwise, returns nullptr.
  void* Allocate(size_t size);

  // Frees a block of memory.
  // If |ptr| belongs to the pool, it is returned to the pool and returns true.
  // Otherwise, returns false.
  bool Free(void* ptr);

  bool IsFromPool(const void* ptr) const;

  size_t block_size() const { return block_size_; }
  size_t total_blocks() const { return total_blocks_; }
  std::string_view name() const { return name_; }

  // For testing
  InfoForTesting GetInfoForTesting() const;

 private:
  void AssertValidPoolPtr(const void* ptr) const;
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

// DualBlockPool manages two FixedBlockPool instances (small and large)
// to support dual-size pooling requirements (e.g. for audio buffers).
// It enforces strict best-fit routing and has no heap fallback.
class DualBlockPool {
 public:
  struct PoolConfig {
    size_t block_size;
    size_t total_blocks;
  };

  struct StateForTesting {
    size_t block_size;
    size_t total_blocks;
    size_t free_blocks;
  };

  DualBlockPool(const std::string& name,
                const PoolConfig& small_config,
                const PoolConfig& large_config);

  ~DualBlockPool();

  // Disallow copy and assign.
  DualBlockPool(const DualBlockPool&) = delete;
  DualBlockPool& operator=(const DualBlockPool&) = delete;

  // Allocates a block of memory from the appropriate pool.
  // Returns nullptr if the request is oversized or the target pool is
  // exhausted.
  void* Allocate(size_t size);

  // Frees a block of memory.
  // Returns true if the pointer was freed by one of the managed pools.
  // Returns false otherwise.
  bool Free(void* ptr);

  std::string_view name() const { return name_; }
  size_t block_size() const;
  size_t total_blocks() const;

  // For testing
  std::vector<StateForTesting> GetStatesForTesting() const;

 private:
  const std::string name_;
  FixedBlockPool small_pool_;
  FixedBlockPool large_pool_;
};

// PooledAllocator wraps any pure pool (FixedBlockPool or DualBlockPool)
// and adds heap fallback logic (::operator new/delete) when the pool
// is disabled, exhausted, or the request is oversized.
template <typename PoolType>
class PooledAllocator {
 public:
  // Constructor using perfect forwarding to construct PoolType in-place.
  template <typename... Args>
  explicit PooledAllocator(Args&&... args)
      : pool_(std::forward<Args>(args)...) {}

  ~PooledAllocator() = default;

  // Disallow copy and assign.
  PooledAllocator(const PooledAllocator&) = delete;
  PooledAllocator& operator=(const PooledAllocator&) = delete;

  // Allocates memory.
  // Attempts to allocate from the pool.
  // If the pool is exhausted or the size is too large,
  // falls back to standard heap allocation (::operator new).
  void* Allocate(size_t size);

  // Frees memory.
  // Attempts to return the memory to the pool. If the pool does not own
  // the pointer, it is safely freed using standard heap deallocation
  // (::operator delete).
  void Free(void* ptr);

  // For testing/access
  const PoolType& pool() const { return pool_; }
  PoolType& pool() { return pool_; }

 private:
  PoolType pool_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_POOLED_ALLOCATOR_H_
