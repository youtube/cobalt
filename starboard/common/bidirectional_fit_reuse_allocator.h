// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_
#define STARBOARD_COMMON_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_

#include <algorithm>
#include <cstddef>

#include "starboard/common/allocator.h"
#include "starboard/common/log.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/configuration.h"

namespace starboard {

// This class uses first-fit allocation strategy to allocate memory block whose
// size is greater than the |small_allocation_threshold|.  It uses last-fit
// strategy to allocate memory block whose size is less than or equal to the
// |small_allocation_threshold| to reduce fragmentation.  Note that the size of
// the memory block for an allocation may exceed the size requested, so an
// allocation with a size close to |small_allocation_threshold| could be treated
// as a large allocation.
// If fragmentation is not an issue, FirstFitReuseAllocator might be a simpler
// alternative.
// Note that when using this class a significant |initial_capacity| should be
// set as otherwise new allocations will almost always allocate from the front
// of the fallback allocator.
template <typename UnderlyingReuseAllocator>
class BidirectionalFitReuseAllocator : public UnderlyingReuseAllocator {
 public:
  typedef typename UnderlyingReuseAllocator::FreeBlockSet::iterator
      FreeBlockIterator;
  typedef typename UnderlyingReuseAllocator::FreeBlockSet::reverse_iterator
      FreeBlockReverseiterator;

  BidirectionalFitReuseAllocator(starboard::Allocator* fallback_allocator,
                                 size_t initial_capacity,
                                 size_t small_allocation_threshold,
                                 size_t allocation_increment)
      : UnderlyingReuseAllocator(fallback_allocator,
                                 initial_capacity,
                                 allocation_increment,
                                 /*max_capacity=*/0),
        small_allocation_threshold_(small_allocation_threshold) {}

  BidirectionalFitReuseAllocator(starboard::Allocator* fallback_allocator,
                                 size_t initial_capacity,
                                 size_t small_allocation_threshold,
                                 size_t allocation_increment,
                                 bool enable_decommit_on_idle,
                                 size_t retain_blocks,
                                 size_t conservative_decommit_blocks,
                                 bool aggressive_decommit_on_suspend = false)
      : UnderlyingReuseAllocator(fallback_allocator,
                                 initial_capacity,
                                 allocation_increment,
                                 /*max_capacity=*/0,
                                 enable_decommit_on_idle,
                                 retain_blocks,
                                 conservative_decommit_blocks,
                                 aggressive_decommit_on_suspend),
        small_allocation_threshold_(small_allocation_threshold) {}

  FreeBlockIterator FindFreeBlock(size_t size,
                                  size_t alignment,
                                  FreeBlockIterator begin,
                                  FreeBlockIterator end,
                                  bool* allocate_from_front) override {
    SB_DCHECK(allocate_from_front);

    *allocate_from_front = size > small_allocation_threshold_;
    if (*allocate_from_front) {
      // Start looking through the free list from the front.
      for (FreeBlockIterator it = begin; it != end; ++it) {
        if (it->CanFulfill(size, alignment)) {
          return it;
        }
      }
    } else {
      // Start looking through the free list from the back.
      FreeBlockReverseiterator rbegin(end);
      FreeBlockReverseiterator rend(begin);
      for (FreeBlockReverseiterator it = rbegin; it != rend; ++it) {
        if (it->CanFulfill(size, alignment)) {
          return --it.base();
        }
      }
    }

    return end;
  }

 private:
  size_t small_allocation_threshold_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_
