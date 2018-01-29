/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_
#define NB_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_

#include "nb/reuse_allocator_base.h"
#include "starboard/configuration.h"

namespace nb {

// This class uses first-fit allocation strategy to allocate memory block whose
// size is greater than the |small_allocation_threshold|.  It uses last-fit
// strategy to allocate memory block whose size is less than or equal to the
// |small_allocation_threshold|.  This allows better fragmentation management.
// If fragmentation is not an issue, FirstFitReuseAllocator might be a simpler
// alternative.
// Note that when using this class a significant |initial_capacity| should be
// set as otherwise new allocations will almost always allocate from the front
// of the fallback allocator.
class BidirectionalFitReuseAllocator : public ReuseAllocatorBase {
 public:
  BidirectionalFitReuseAllocator(Allocator* fallback_allocator,
                                 std::size_t initial_capacity,
                                 std::size_t small_allocation_threshold,
                                 std::size_t allocation_increment = 0,
                                 std::size_t max_capacity = 0);

  FreeBlockSet::iterator FindFreeBlock(std::size_t size,
                                       std::size_t alignment,
                                       FreeBlockSet::iterator begin,
                                       FreeBlockSet::iterator end,
                                       bool* allocate_from_front) override;

 private:
  std::size_t small_allocation_threshold_;
};

}  // namespace nb

#endif  // NB_BIDIRECTIONAL_FIT_REUSE_ALLOCATOR_H_
