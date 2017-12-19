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

#include "nb/bidirectional_fit_reuse_allocator.h"

#include <algorithm>

#include "nb/pointer_arithmetic.h"
#include "starboard/log.h"
#include "starboard/types.h"

namespace nb {

BidirectionalFitReuseAllocator::BidirectionalFitReuseAllocator(
    Allocator* fallback_allocator,
    std::size_t initial_capacity,
    std::size_t small_allocation_threshold,
    std::size_t allocation_increment /*= 0*/,
    std::size_t max_capacity /* =0 */
    )
    : ReuseAllocatorBase(fallback_allocator,
                         initial_capacity,
                         allocation_increment,
                         max_capacity),
      small_allocation_threshold_(small_allocation_threshold) {}

ReuseAllocatorBase::FreeBlockSet::iterator
BidirectionalFitReuseAllocator::FindFreeBlock(std::size_t size,
                                              std::size_t alignment,
                                              FreeBlockSet::iterator begin,
                                              FreeBlockSet::iterator end,
                                              bool* allocate_from_front) {
  SB_DCHECK(allocate_from_front);

  *allocate_from_front = size > small_allocation_threshold_;

  if (*allocate_from_front) {
    // Start looking through the free list from the front.
    for (FreeBlockSet::iterator it = begin; it != end; ++it) {
      if (it->CanFullfill(size, alignment)) {
        return it;
      }
    }
  }

  // Start looking through the free list from the back.
  FreeBlockSet::reverse_iterator rbegin(end);
  FreeBlockSet::reverse_iterator rend(begin);
  for (FreeBlockSet::reverse_iterator it = rbegin; it != rend; ++it) {
    if (it->CanFullfill(size, alignment)) {
      return --it.base();
    }
  }

  return end;
}

}  // namespace nb
