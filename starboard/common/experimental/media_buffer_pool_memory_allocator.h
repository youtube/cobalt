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
#include <cstddef>

#include "starboard/common/allocator.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/log.h"
#include "starboard/common/pointer_arithmetic.h"

namespace starboard {
namespace experimental {

// MediaBufferPoolMemoryAllocator allocates memory sequentially and expands the
// underlying MediaBufferPool as needed.
// It does not support freeing individual allocations (Free is a no-op).
class MediaBufferPoolMemoryAllocator : public starboard::Allocator {
 public:
  explicit MediaBufferPoolMemoryAllocator(
      starboard::experimental::MediaBufferPool* pool)
      : pool_(pool) {
    SB_DCHECK(pool_);
  }

  ~MediaBufferPoolMemoryAllocator() { pool_->ShrinkToZero(); }

  void* Allocate(size_t size) override { return Allocate(size, kMinAlignment); }

  void* Allocate(size_t size, size_t alignment) override {
    SB_DCHECK(pool_);

    if (size == 0) {
      return nullptr;
    }

    alignment = std::max(alignment, kMinAlignment);

    // Align the current offset.
    size_t aligned_offset = AlignUp(offset_, alignment);
    size_t padding = aligned_offset - offset_;

    size_t new_offset = offset_ + padding + size;
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

  void* AllocateForAlignment(size_t* size, size_t alignment) override {
    return Allocate(*size, alignment);
  }

  void Free(void* memory) override {
    // Free is a no-op.
  }

  size_t GetCapacity() const override {
    // Returns 0 here to avoid tracking the allocated memory twice if used
    // with another allocator that tracks capacity.
    return 0;
  }

  size_t GetAllocated() const override { return offset_; }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override {}

 private:
  starboard::experimental::MediaBufferPool* pool_;
  // Not start from 0, so it won't return `nullptr` as a valid pointer.
  size_t offset_ = sizeof(void*);
};

}  // namespace experimental
}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_MEMORY_ALLOCATOR_H_
