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

#ifndef NB_FIRST_FIT_REUSE_ALLOCATOR_H_
#define NB_FIRST_FIT_REUSE_ALLOCATOR_H_

#include "nb/reuse_allocator_base.h"
#include "starboard/configuration.h"

namespace nb {

// This class uses first-fit allocation strategy to allocate memory.
class FirstFitReuseAllocator : public ReuseAllocatorBase {
 public:
  FirstFitReuseAllocator(Allocator* fallback_allocator,
                         std::size_t initial_capacity,
                         std::size_t allocation_increment = 0);

  FreeBlockSet::iterator FindFreeBlock(std::size_t size,
                                       std::size_t alignment,
                                       FreeBlockSet::iterator begin,
                                       FreeBlockSet::iterator end,
                                       bool* allocate_from_front) override;
};

}  // namespace nb

#endif  // NB_FIRST_FIT_REUSE_ALLOCATOR_H_
