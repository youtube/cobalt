/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "nb/memory_pool.h"

#include "starboard/log.h"

namespace nb {

MemoryPool::MemoryPool(void* buffer, std::size_t size, bool thread_safe)
    : no_free_allocator_(buffer, size),
      reuse_allocator_(
          scoped_ptr<Allocator>(new ReuseAllocator(&no_free_allocator_)),
          thread_safe) {
  SB_DCHECK(buffer);
  SB_DCHECK(size != 0U);

  // Try to allocate the whole budget and free it immediately.  This can:
  // 1. Ensure the size is accurate after accounting for all implicit alignment
  //    enforced by the underlying allocators.
  // 2. The |reuse_allocator_| contains a free block of the whole budget.  As
  //    the |reuse_allocator_| doesn't support extending of free block, an
  //    allocation that is larger than the both biggest free block in the
  //   |reuse_allocator_| and the remaining memory inside the
  //   |no_free_allocator_| will fail even the combination of both can fulfill
  //   the allocation.
  void* p = Allocate(size);
  SB_DCHECK(p);
  Free(p);
}

void MemoryPool::PrintAllocations() const {
  reuse_allocator_.PrintAllocations();
}

}  // namespace nb
