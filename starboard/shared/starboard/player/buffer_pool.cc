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

#include "starboard/shared/starboard/player/buffer_pool.h"

#include "starboard/common/check_op.h"
#include "starboard/common/once.h"

namespace starboard {

BufferPool::BufferPool(size_t buffer_size, size_t pool_size)
    : buffer_size_(buffer_size),
      pool_size_(pool_size),
      pool_storage_(new uint8_t[buffer_size * pool_size]) {
  SB_CHECK_GT(buffer_size_, 0U);
  SB_CHECK_GT(pool_size_, 0U);
  free_list_.reserve(pool_size_);
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.push_back(pool_storage_.get() + i * buffer_size_);
  }
  SB_LOG(INFO) << "BufferPool is created: buffer size=" << buffer_size_
               << ", pool size=" << pool_size_;
}

BufferPool::~BufferPool() {
  SB_DCHECK_EQ(free_list_.size(), pool_size_)
      << "Not all buffers were returned to the pool.";
}

uint8_t* BufferPool::Allocate(size_t size) {
  SB_CHECK_LE(size, buffer_size_);
  std::lock_guard<std::mutex> lock(mutex_);
  if (!free_list_.empty()) {
    uint8_t* ptr = free_list_.back();
    free_list_.pop_back();
    return ptr;
  }
  return nullptr;
}

void BufferPool::Free(uint8_t* ptr, size_t size) {
  SB_CHECK_LE(size, buffer_size_);
  SB_CHECK(IsFromPool(ptr));
  std::lock_guard<std::mutex> lock(mutex_);
  SB_CHECK_LT(free_list_.size(), pool_size_);
  free_list_.push_back(ptr);
}

bool BufferPool::IsFromPool(uint8_t* ptr) const {
  uintptr_t ptr_val = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t start_val = reinterpret_cast<uintptr_t>(pool_storage_.get());
  uintptr_t end_val = start_val + buffer_size_ * pool_size_;
  return ptr_val >= start_val && ptr_val < end_val &&
         (ptr_val - start_val) % buffer_size_ == 0;
}

}  // namespace starboard
