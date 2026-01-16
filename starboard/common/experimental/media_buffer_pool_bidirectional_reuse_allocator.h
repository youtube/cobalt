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

#ifndef STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_BIDIRECTIONAL_REUSE_ALLOCATOR_H_
#define STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_BIDIRECTIONAL_REUSE_ALLOCATOR_H_

#include <cstddef>

#include "starboard/common/allocator.h"
#include "starboard/common/bidirectional_fit_reuse_allocator.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/experimental/media_buffer_pool_memory_allocator.h"
#include "starboard/common/reuse_allocator_base.h"

namespace starboard {
namespace common {
namespace experimental {

// MediaBufferPoolBidirectionalReuseAllocator uses
// MediaBufferPoolMemoryAllocator as the fallback allocator and provides
// bidirectional fit reuse strategy.
class MediaBufferPoolBidirectionalReuseAllocator : public Allocator {
 public:
  MediaBufferPoolBidirectionalReuseAllocator(
      MediaBufferPool* pool,
      std::size_t initial_capacity,
      std::size_t small_allocation_threshold,
      std::size_t allocation_increment)
      : fallback_allocator_(pool),
        bidirectional_fit_reuse_allocator_(&fallback_allocator_,
                                           initial_capacity,
                                           small_allocation_threshold,
                                           allocation_increment) {}

  ~MediaBufferPoolBidirectionalReuseAllocator() {
    SB_DCHECK_EQ(GetAllocated(), 0);
  }

  void* Allocate(std::size_t size) override {
    return AnnotatePointer(bidirectional_fit_reuse_allocator_.Allocate(size));
  }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    return AnnotatePointer(
        bidirectional_fit_reuse_allocator_.Allocate(size, alignment));
  }

  void* AllocateForAlignment(size_t* size, size_t alignment) override {
    return AnnotatePointer(
        bidirectional_fit_reuse_allocator_.AllocateForAlignment(size,
                                                                alignment));
  }

  void Free(void* memory) override {
    bidirectional_fit_reuse_allocator_.Free(UnannotatePointer(memory));
  }

  void FreeWithSize(void* memory, size_t size) override {
    bidirectional_fit_reuse_allocator_.FreeWithSize(UnannotatePointer(memory),
                                                    size);
  }

  size_t GetCapacity() const override {
    return bidirectional_fit_reuse_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return bidirectional_fit_reuse_allocator_.GetAllocated();
  }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override {
    bidirectional_fit_reuse_allocator_.PrintAllocations(
        align_allocated_size, max_allocations_to_print);
  }

 private:
  MediaBufferPoolMemoryAllocator fallback_allocator_;
  BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      bidirectional_fit_reuse_allocator_;
};

}  // namespace experimental
}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_EXPERIMENTAL_MEDIA_BUFFER_POOL_BIDIRECTIONAL_REUSE_ALLOCATOR_H_
