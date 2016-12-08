/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_MEDIA_BUFFER_ALLOCATOR_H_
#define COBALT_MEDIA_MEDIA_BUFFER_ALLOCATOR_H_

#include "base/optional.h"
#include "nb/memory_pool.h"

namespace cobalt {
namespace media {

class MediaBufferAllocator {
 public:
  MediaBufferAllocator(void* pool, size_t main_pool_size,
                       size_t small_allocation_pool_size,
                       size_t small_allocation_threshold)
      : pool_(reinterpret_cast<uint8*>(pool)),
        main_pool_size_(main_pool_size),
        small_allocation_pool_size_(small_allocation_pool_size),
        small_allocation_threshold_(small_allocation_threshold),
        main_pool_(pool_, main_pool_size_, true, /* thread_safe */
                   true /* verify_full_capacity */) {
    if (small_allocation_pool_size_ > 0u) {
      DCHECK_GT(small_allocation_threshold_, 0u);
      small_allocation_pool_.emplace(pool_ + main_pool_size_,
                                     small_allocation_pool_size_,
                                     true, /* thread_safe */
                                     true /* verify_full_capacity */);
    } else {
      DCHECK_EQ(small_allocation_pool_size_, 0u);
      DCHECK_EQ(small_allocation_threshold_, 0u);
    }
  }

  void* Allocate(size_t size, size_t alignment) {
    void* p = NULL;
    if (size < small_allocation_threshold_ && small_allocation_pool_) {
      p = small_allocation_pool_->Allocate(size, alignment);
    }
    if (!p) {
      p = main_pool_.Allocate(size, alignment);
    }
    if (!p && small_allocation_pool_) {
      p = small_allocation_pool_->Allocate(size, alignment);
    }
    return p;
  }

  void Free(void* p) {
    if (p >= pool_ + main_pool_size_ && small_allocation_pool_) {
      DCHECK_LT(p, pool_ + main_pool_size_ + small_allocation_pool_size_);
      small_allocation_pool_->Free(p);
      return;
    }
    DCHECK_GE(p, pool_);
    DCHECK_LT(p, pool_ + main_pool_size_);
    main_pool_.Free(p);
  }

 private:
  uint8* pool_;
  size_t main_pool_size_;
  size_t small_allocation_pool_size_;
  size_t small_allocation_threshold_;

  nb::MemoryPool main_pool_;
  base::optional<nb::MemoryPool> small_allocation_pool_;

  DISALLOW_COPY_AND_ASSIGN(MediaBufferAllocator);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_BUFFER_ALLOCATOR_H_
