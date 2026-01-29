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

#ifndef STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_MEMORY_ALLOCATOR_H_
#define STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_MEMORY_ALLOCATOR_H_

#include <algorithm>

#include "starboard/common/allocator.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/log.h"

namespace starboard {
namespace common {
namespace experimental {

// MediaBufferPoolMemoryAllocator allocates memory sequentially and expands the
// underlying MediaBufferPool as needed.
// It does not support freeing individual allocations (Free is a no-op).
class MediaBufferPoolMemoryAllocator : public starboard::common::Allocator {
 public:
  explicit MediaBufferPoolMemoryAllocator(
      starboard::common::experimental::MediaBufferPool* pool)
      : pool_(pool) {
    SB_DCHECK(pool_);
  }

  ~MediaBufferPoolMemoryAllocator() { pool_->ShrinkToZero(); }

  void* Allocate(std::size_t size) override { return Allocate(size, 1); }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    SB_DCHECK(pool_);

    if (size == 0) {
      return nullptr;
    }

    // Align the current offset.
    std::size_t padding = 0;
    if (alignment > 1) {
      uintptr_t current_address = offset_;
      uintptr_t aligned_address =
          (current_address + alignment - 1) & ~(alignment - 1);
      padding = aligned_address - current_address;
    }

    std::size_t new_offset = offset_ + padding + size;
    if (!pool_->ExpandTo(new_offset)) {
      SB_LOG(WARNING) << "Failed to expand MediaBufferPool to " << new_offset;
      return nullptr;
    }

    SB_LOG(INFO) << "Expanded MediaBufferPool to " << new_offset;
    offset_ += padding;
    void* ptr = reinterpret_cast<void*>(offset_);
    offset_ += size;

    return ptr;
  }

  void* AllocateForAlignment(std::size_t* size,
                             std::size_t alignment) override {
    return Allocate(*size, alignment);
  }

  void Free(void* memory) override {
    // Free is a no-op.
  }

  std::size_t GetCapacity() const override {
    // Returns 0 here to avoid tracking the allocated memory twice if used
    // with another allocator that tracks capacity.
    return 0;
  }

  std::size_t GetAllocated() const override { return offset_; }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override {}

 private:
  starboard::common::experimental::MediaBufferPool* pool_;
  // Not start from 0, so it won't return `nullptr` as a valid pointer.
  std::size_t offset_ = sizeof(void*);
};

}  // namespace experimental
}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_MEMORY_ALLOCATOR_H_
