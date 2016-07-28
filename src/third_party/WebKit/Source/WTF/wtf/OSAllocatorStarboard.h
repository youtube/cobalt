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

// Adapted from OSAllocatorShell.h

#include "config.h"

#include <set>

#include <wtf/OSAllocator.h>
#include <wtf/ThreadingPrimitives.h>

#include "starboard/memory.h"

// TODO: Replace this with a app-level configuration item. Need to move CVals
// somewhere before we can enable this.
#undef MEMORY_STATISTICS

namespace WTF {

class StarboardPageAllocator {
 public:

    StarboardPageAllocator()
#if defined(MEMORY_STATISTICS)
        : current_bytes_allocated_(
            "WebKit.StarboardPageAllocator.BytesAllocated", 0,
            "Number of bytes allocated through the StarboardPageAllocator.")
        , pages_allocated_(
            "WebKit.StarboardPageAllocator.PagesAllocated", 0,
            "Number of pages currently allocated, even if they are not "
            "allocated via the StarboardPageAllocator (e.g. its capacity is "
            "exceeded).")
#endif  // defined(MEMORY_STATISTICS)
    {
        kPageSize = SB_MEMORY_PAGE_SIZE;
    }

    virtual ~StarboardPageAllocator() {
    instance_ = NULL;
  }

  void* allocate(size_t alignment, size_t size) {
    MutexLocker lock(mutex_);
    return allocateInternal(alignment, size);
  }

  void free(void* p, size_t size) {
    MutexLocker lock(mutex_);
    freeInternal(p, size);
  }

  static StarboardPageAllocator* getInstance();

  static bool instanceExists() {
    return instance_ != NULL;
  }

  size_t getCurrentBytesAllocated() const {
#if defined(MEMORY_STATISTICS)
    return current_bytes_allocated_;
#else
    return 0;
#endif
  }

  static size_t kPageSize;

 protected:

  virtual void* allocateInternal(size_t alignment, size_t size) = 0;
  virtual void freeInternal(void* p, size_t size) = 0;

  void updateAllocatedBytes(ssize_t bytes) {
#if defined(MEMORY_STATISTICS)
    current_bytes_allocated_ += bytes;
#endif
  }

  void updateAllocatedPages(ssize_t page_count) {
#if defined(MEMORY_STATISTICS)
    pages_allocated_ += page_count;
#endif
  }

  static StarboardPageAllocator* instance_;

 private:
#if defined(MEMORY_STATISTICS)
  LB::CVal<size_t> current_bytes_allocated_;
  LB::CVal<size_t> pages_allocated_;
#endif
  WTF::Mutex mutex_;

};

#if SB_HAS(MMAP)
// Allocator that uses lb_mmap for page-aligned allocations.
// Uses memalign for non-page-aligned allocations.
class StarboardPageAllocatorMmap : public StarboardPageAllocator {
 public:
  static StarboardPageAllocatorMmap* Create() {
    return new StarboardPageAllocatorMmap();
  }

 protected:
  void* allocateInternal(size_t alignment, size_t size) OVERRIDE;
  void freeInternal(void* addr, size_t size) OVERRIDE;

 private:
  StarboardPageAllocatorMmap();
  // Set of allocations that were malloced rather than mmapped.
  // These addresses should be freed rather than munmapped.
  std::set<void*> non_page_blocks_;
};
#endif  // defined(LB_HAS_MMAP)

// Special case allocator for managing a fixed pool of 64K-size, 64K-aligned
// blocks.  JavaScript heap is a big consumer of these.
class StarboardPageAllocatorFixed : public StarboardPageAllocator {
 public:
  ~StarboardPageAllocatorFixed() {
    SbMemoryFree(buffer_orig_);
    SbMemoryFree(free_bitmap_);
  }

  static StarboardPageAllocatorFixed* Create() {
    return new StarboardPageAllocatorFixed();
  }

 protected:
  void* allocateInternal(size_t alignment, size_t size) OVERRIDE;
  void freeInternal(void* p, size_t size) OVERRIDE;

 private:
  void* allocatePage();
  void freePage(void* p);
  bool isValidPage(void* p) const;
  // Use the system allocator to get an aligned block.
  void* allocateBlock(size_t alignment, size_t size);

  // Free a block that was allocated with allocateBlock()
  void freeBlock(void* ptr) {
    void* orig_ptr = *(void**)((uintptr_t)ptr - sizeof(void*));
    SbMemoryFree(orig_ptr);
  }

  void* buffer_;
  void* buffer_orig_;
  size_t buffer_size_;
  uint32_t* free_bitmap_;
  int bitmap_size_;

  // Records how many pages couldn't fit in the buffer.
  // We can use this to size our buffer appropriately.
  // The high water mark is the maximum number of page requests
  // we couldn't fulfill for the current run.
#if defined(MEMORY_STATISTICS)
  LB::CVal<int> excess_page_count_;
  LB::CVal<int> excess_high_water_mark_;
#endif

  StarboardPageAllocatorFixed();

  static inline uint32_t align(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
  }

  static inline void* alignPtr(void* ptr, size_t alignment) {
    return (void*)(((uintptr_t)ptr + alignment - 1) & ~(alignment - 1));
  }

  void set_bit(uint32_t index, uint32_t bit) {
    ASSERT(static_cast<int>(index) < bitmap_size_);
    free_bitmap_[index] |= (1 << bit);
  }

  void clear_bit(uint32_t index, uint32_t bit) {
    ASSERT(static_cast<int>(index) < bitmap_size_);
    free_bitmap_[index] &= ~(1 << bit);
  }
};

}  // namespace WTF
