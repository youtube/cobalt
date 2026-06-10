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

#include "starboard/shared/starboard/player/fixed_block_pool.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

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

FixedBlockPool::FixedBlockPool(std::string_view name,
                               size_t block_size,
                               size_t total_blocks)
    : name_(name),
      block_size_(AlignBlockSize(block_size)),
      total_blocks_(ValidateTotalBlocks(total_blocks)),
      pool_storage_(AllocatePoolStorage(block_size_, total_blocks_)) {
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
      return ptr;
    }
  }

  // Fallback to heap
  if constexpr (kStatusLogIntervalUs.has_value()) {
    SB_LOG(WARNING) << "heap alloc will happen: name=" << name_ << ", reason="
                    << (size > block_size_ ? "oversized" : "exhausted")
                    << ", requested_size(KiB)=" << (size / 1024)
                    << ", block_size(KiB)=" << (block_size_ / 1024)
                    << ", total_blocks=" << total_blocks_;
  }
  return ::operator new(size);
}

void FixedBlockPool::Free(void* ptr) {
  if (!ptr) {
    return;
  }

  if (IsFromPool(ptr)) {
    std::lock_guard lock(mutex_);
    SB_CHECK_LT(free_list_.size(), total_blocks_);
    free_list_.push_back(ptr);
  } else {
    ::operator delete(ptr);
  }

  MaybeLogStatus();
}

void FixedBlockPool::MaybeLogStatus() {
  if constexpr (kStatusLogIntervalUs.has_value()) {
    int64_t now = CurrentMonotonicTime();
    size_t free_blocks = 0;
    bool should_log = false;

    {
      std::lock_guard lock(mutex_);
      int64_t elapsed = now - last_status_logged_us_;
      if (elapsed >= kStatusLogIntervalUs.value()) {
        free_blocks = free_list_.size();
        last_status_logged_us_ = now;
        should_log = true;
      }
    }

    if (should_log) {
      SB_LOG(INFO) << "FixedBlockPool status: name=" << name_
                   << ", free_blocks=" << free_blocks
                   << ", total_blocks=" << total_blocks_ << " (fullness: "
                   << (total_blocks_ - free_blocks) * 100 / total_blocks_
                   << "%)";
    }
  }
}

bool FixedBlockPool::IsFromPool(const void* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());
  uintptr_t end_val = start_val + block_size_ * total_blocks_;
  return ptr_val >= start_val && ptr_val < end_val &&
         (ptr_val - start_val) % block_size_ == 0;
}

FixedBlockPool::InfoForTesting FixedBlockPool::GetInfoForTesting() const {
  std::lock_guard lock(mutex_);
  return InfoForTesting{block_size_, total_blocks_, free_list_.size()};
}

}  // namespace starboard
