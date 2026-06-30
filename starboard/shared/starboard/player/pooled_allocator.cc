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

#include "starboard/shared/starboard/player/pooled_allocator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/common/time.h"

namespace starboard {

namespace {

// When set, periodically logs the fullness status of the pool at the
// specified interval in microseconds, and enables heap fallback warnings.
// std::nullopt disables all logging and warnings.
constexpr std::optional<int64_t> kStatusLogIntervalUs = std::nullopt;

size_t AlignBlockSize(size_t block_size) {
  SB_CHECK_GT(block_size, 0U);
  return AlignUp(block_size, alignof(std::max_align_t));
}

size_t ValidateTotalBlocks(size_t total_blocks) {
  SB_CHECK_GT(total_blocks, 0U);
  return total_blocks;
}

std::unique_ptr<uint8_t[]> AllocatePoolStorage(size_t block_size,
                                               size_t total_blocks) {
  SB_CHECK_LE(block_size, std::numeric_limits<size_t>::max() / total_blocks);
  std::unique_ptr<uint8_t[]> storage(new uint8_t[block_size * total_blocks]);
  SB_CHECK(storage);
  return storage;
}

}  // namespace

// ============================================================================
// FixedBlockPool Implementation
// ============================================================================

namespace internal {

FixedBlockPool::FixedBlockPool(std::string_view name,
                               size_t block_size,
                               size_t total_blocks)
    : name_(name),
      block_size_(AlignBlockSize(block_size)),
      total_blocks_(ValidateTotalBlocks(total_blocks)),
      pool_storage_(AllocatePoolStorage(block_size_, total_blocks_)),
      is_block_allocated_(total_blocks_, false) {
  free_list_.reserve(total_blocks_);

  uint8_t* base = pool_storage_.get();
  for (size_t i = 0; i < total_blocks_; ++i) {
    free_list_.push_back(base + i * block_size_);
  }
  SB_LOG(INFO) << "FixedBlockPool created: name=" << name_
               << ", block_size(KiB)=" << block_size_ / 1024
               << ", total_blocks=" << total_blocks_;
}

FixedBlockPool::~FixedBlockPool() {
  SB_DCHECK_EQ(free_list_.size(), total_blocks_)
      << "Not all blocks were returned to the pool.";
}

void* FixedBlockPool::TryAllocate() {
  std::lock_guard lock(mutex_);
  if (free_list_.empty()) {
    if constexpr (kStatusLogIntervalUs.has_value()) {
      SB_LOG(WARNING) << "heap alloc will happen: reason=exhausted"
                      << ", name=" << name_
                      << ", block_size(KiB)=" << (block_size_ / 1024)
                      << ", total_blocks=" << total_blocks_;
    }
    return nullptr;
  }

  void* ptr = free_list_.back();
  free_list_.pop_back();

  size_t block_index = GetBlockIndex(ptr);
  SB_CHECK(!is_block_allocated_[block_index])
      << "Internal error: block already marked as allocated: " << ptr;
  is_block_allocated_[block_index] = true;

  return ptr;
}

bool FixedBlockPool::TryFree(void* ptr) {
  if (!ptr) {
    return false;
  }

  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());
  uintptr_t end_val = start_val + block_size_ * total_blocks_;

  if (ptr_val < start_val || ptr_val >= end_val) {
    return false;
  }

  std::lock_guard lock(mutex_);

  size_t block_index = GetBlockIndex(ptr);
  SB_CHECK(is_block_allocated_[block_index])
      << "Double free detected in FixedBlockPool [" << name_
      << "] for pointer: " << ptr;

  is_block_allocated_[block_index] = false;

  SB_CHECK_LT(free_list_.size(), total_blocks_);
  free_list_.push_back(ptr);
  MaybeLogStatus_Locked();
  return true;
}

FixedBlockPool::InfoForTesting FixedBlockPool::GetInfoForTesting() const {
  std::lock_guard lock(mutex_);
  return InfoForTesting{block_size_, total_blocks_, free_list_.size()};
}

size_t FixedBlockPool::GetBlockIndex(const void* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());

  SB_CHECK_GE(ptr_val, start_val);
  SB_CHECK_LT(ptr_val, start_val + block_size_ * total_blocks_);
  SB_CHECK(IsAligned(ptr_val - start_val, block_size_))
      << "Pointer is misaligned inside pool range: " << ptr;

  return (ptr_val - start_val) / block_size_;
}

void FixedBlockPool::MaybeLogStatus_Locked() {
  if constexpr (!kStatusLogIntervalUs.has_value()) {
    return;
  }

  int64_t now = CurrentMonotonicTime();
  int64_t elapsed = now - last_status_logged_us_;
  if (elapsed < kStatusLogIntervalUs.value()) {
    return;
  }

  size_t free_blocks = free_list_.size();
  last_status_logged_us_ = now;

  SB_LOG(INFO) << "FixedBlockPool status: name=" << name_
               << ", free_blocks=" << free_blocks
               << ", total_blocks=" << total_blocks_ << " (fullness: "
               << (total_blocks_ - free_blocks) * 100 / total_blocks_ << "%)";
}

}  // namespace internal

PooledAllocator::PooledAllocator(std::vector<PoolConfig> configs) {
  // Sort configs by block size ascending to guarantee best-fit routing.
  std::sort(configs.begin(), configs.end(),
            [](const PoolConfig& a, const PoolConfig& b) {
              return a.block_size < b.block_size;
            });

  for (const auto& config : configs) {
    pools_.push_back(std::make_unique<internal::FixedBlockPool>(
        config.name, config.block_size, config.total_blocks));
  }
}

PooledAllocator::PooledAllocator(std::string_view name,
                                 size_t block_size,
                                 size_t total_blocks) {
  pools_.push_back(std::make_unique<internal::FixedBlockPool>(name, block_size,
                                                              total_blocks));
}

PooledAllocator::~PooledAllocator() = default;

void* PooledAllocator::Allocate(size_t size) {
  if (size == 0) {
    // For size of 0, we use default heap allocator.
    return ::operator new(size);
  }

  for (const auto& pool : pools_) {
    if (size <= pool->block_size()) {
      void* ptr = pool->TryAllocate();
      if (ptr) {
        return ptr;
      }
      // Pool matched size requirement but is exhausted -> fallback to heap.
      return ::operator new(size);
    }
  }

  // Oversized allocation (exceeds all configured pools).
  if constexpr (kStatusLogIntervalUs.has_value()) {
    if (!pools_.empty() && size > pools_.back()->block_size()) {
      const auto& last_pool = pools_.back();
      SB_LOG(WARNING) << "heap alloc will happen: reason=oversized"
                      << ", name=" << last_pool->name()
                      << ", requested_size(KiB)=" << (size / 1024)
                      << ", block_size(KiB)="
                      << (last_pool->block_size() / 1024)
                      << ", total_blocks=" << last_pool->total_blocks();
    }
  }

  return ::operator new(size);
}

void PooledAllocator::Free(void* ptr) {
  if (!ptr) {
    return;
  }

  for (const auto& pool : pools_) {
    if (pool->TryFree(ptr)) {
      return;
    }
  }

  ::operator delete(ptr);
}

}  // namespace starboard
