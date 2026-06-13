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

void* FixedBlockPool::Allocate(size_t size) {
  if (size <= block_size_) {
    std::lock_guard lock(mutex_);
    if (!free_list_.empty()) {
      void* ptr = free_list_.back();
      free_list_.pop_back();

      AssertValidPoolPtr(ptr);
      size_t block_index = GetBlockIndex(ptr);
      SB_CHECK(!is_block_allocated_[block_index])
          << "Internal error: block already marked as allocated: " << ptr;
      is_block_allocated_[block_index] = true;

      return ptr;
    }
  }
  return nullptr;
}

bool FixedBlockPool::Free(void* ptr) {
  if (!ptr) {
    return false;
  }

  if (IsFromPool(ptr)) {
    std::lock_guard lock(mutex_);

    AssertValidPoolPtr(ptr);  // Explicit validation!

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

  // If IsFromPool returned false, it could be a normal heap pointer OR a
  // misaligned pool pointer. We must assert that the pointer is NOT inside
  // the pool range (i.e. if it is >= start_val, it must be >= end_val).
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());
  uintptr_t end_val = start_val + block_size_ * total_blocks_;

  if (ptr_val >= start_val) {
    SB_CHECK_GE(ptr_val, end_val)
        << "Attempted to free a misaligned pointer inside the pool range ["
        << name_ << "]: " << ptr;
  }

  return false;
}

bool FixedBlockPool::IsFromPool(const void* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());
  uintptr_t end_val = start_val + block_size_ * total_blocks_;
  return ptr_val >= start_val && ptr_val < end_val &&
         IsAligned(ptr_val - start_val, block_size_);
}

FixedBlockPool::InfoForTesting FixedBlockPool::GetInfoForTesting() const {
  std::lock_guard lock(mutex_);
  return InfoForTesting{block_size_, total_blocks_, free_list_.size()};
}

void FixedBlockPool::AssertValidPoolPtr(const void* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());

  SB_CHECK_GE(ptr_val, start_val);
  SB_CHECK_LT(ptr_val, start_val + block_size_ * total_blocks_);
  SB_CHECK(IsAligned(ptr_val - start_val, block_size_))
      << "Pointer is misaligned inside pool range: " << ptr;
}

size_t FixedBlockPool::GetBlockIndex(const void* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());

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

// ============================================================================
// DualBlockPool Implementation
// ============================================================================

DualBlockPool::DualBlockPool(const std::string& name,
                             const PoolConfig& small_config,
                             const PoolConfig& large_config)
    : name_(name),
      small_pool_(name + "_Small",
                  small_config.block_size,
                  small_config.total_blocks),
      large_pool_(name + "_Large",
                  large_config.block_size,
                  large_config.total_blocks) {}

DualBlockPool::~DualBlockPool() = default;

void* DualBlockPool::Allocate(size_t size) {
  if (size <= small_pool_.block_size()) {
    return small_pool_.Allocate(size);
  }
  if (size <= large_pool_.block_size()) {
    return large_pool_.Allocate(size);
  }
  return nullptr;
}

bool DualBlockPool::Free(void* ptr) {
  if (small_pool_.Free(ptr)) {
    return true;
  }
  return large_pool_.Free(ptr);
}

size_t DualBlockPool::block_size() const {
  return large_pool_.block_size();
}

size_t DualBlockPool::total_blocks() const {
  return small_pool_.total_blocks() + large_pool_.total_blocks();
}

std::vector<DualBlockPool::StateForTesting> DualBlockPool::GetStatesForTesting()
    const {
  return {
      {small_pool_.block_size(), small_pool_.GetInfoForTesting().total_blocks,
       small_pool_.GetInfoForTesting().free_blocks},
      {large_pool_.block_size(), large_pool_.GetInfoForTesting().total_blocks,
       large_pool_.GetInfoForTesting().free_blocks}};
}

// ============================================================================
// PooledAllocator Implementation
// ============================================================================

template <typename PoolType>
void* PooledAllocator<PoolType>::Allocate(size_t size) {
  if (size > 0) {
    void* ptr = pool_.Allocate(size);
    if (ptr) {
      return ptr;
    }
  }
  if constexpr (kStatusLogIntervalUs.has_value()) {
    SB_LOG(WARNING) << "heap alloc will happen: name=" << pool_.name()
                    << ", reason="
                    << (size > pool_.block_size() ? "oversized" : "exhausted")
                    << ", requested_size(KiB)=" << (size / 1024)
                    << ", block_size(KiB)=" << (pool_.block_size() / 1024)
                    << ", total_blocks=" << pool_.total_blocks();
  }
  return ::operator new(size);
}

template <typename PoolType>
void PooledAllocator<PoolType>::Free(void* ptr) {
  if (!ptr) {
    return;
  }
  if (pool_.Free(ptr)) {
    return;
  }
  ::operator delete(ptr);
}

// Explicit instantiations
template class PooledAllocator<FixedBlockPool>;
template class PooledAllocator<DualBlockPool>;

}  // namespace starboard
