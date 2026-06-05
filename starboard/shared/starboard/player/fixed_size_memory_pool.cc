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

#include "starboard/shared/starboard/player/fixed_size_memory_pool.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/pointer_arithmetic.h"

namespace starboard {

FixedSizeMemoryPool::FixedSizeMemoryPool(size_t block_size, size_t capacity)
    : block_size_([&] {
        SB_CHECK_GT(block_size, 0U);
        return AlignUp(block_size, alignof(std::max_align_t));
      }()),
      capacity_([&] {
        SB_CHECK_GT(capacity, 0U);
        return capacity;
      }()),
      pool_storage_([&] {
        SB_CHECK_LE(block_size_,
                    std::numeric_limits<size_t>::max() / capacity_);
        void* storage = ::operator new(block_size_ * capacity_);
        SB_CHECK(storage);
        return storage;
      }()) {
  free_list_.reserve(capacity_);

  uint8_t* base = static_cast<uint8_t*>(pool_storage_);
  for (size_t i = 0; i < capacity_; ++i) {
    free_list_.push_back(base + i * block_size_);
  }
}

FixedSizeMemoryPool::~FixedSizeMemoryPool() {
  SB_DCHECK_EQ(free_list_.size(), capacity_)
      << "Not all blocks were returned to the pool.";
  ::operator delete(pool_storage_);
}

void* FixedSizeMemoryPool::Allocate() {
  std::lock_guard lock(mutex_);
  if (free_list_.empty()) {
    return nullptr;
  }

  void* ptr = free_list_.back();
  free_list_.pop_back();
  return ptr;
}

void FixedSizeMemoryPool::Free(void* ptr) {
  if (!ptr) {
    return;
  }

  SB_CHECK(IsFromPool(ptr));
  std::lock_guard lock(mutex_);
  SB_CHECK_LT(free_list_.size(), capacity_);
  free_list_.push_back(ptr);
}

bool FixedSizeMemoryPool::IsFromPool(void* ptr) const {
  if (!pool_storage_) {
    return false;
  }
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_);
  uintptr_t end_val = start_val + block_size_ * capacity_;
  return ptr_val >= start_val && ptr_val < end_val &&
         (ptr_val - start_val) % block_size_ == 0;
}

size_t FixedSizeMemoryPool::free_list_size() const {
  std::lock_guard lock(mutex_);
  return free_list_.size();
}

}  // namespace starboard
