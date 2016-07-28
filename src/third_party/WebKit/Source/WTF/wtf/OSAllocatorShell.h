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
#include "config.h"

#include <set>

#include <wtf/OSAllocator.h>
#include <wtf/ThreadingPrimitives.h>

#include "lb_memory_pages.h"

namespace WTF {

class ShellPageAllocator {
 public:
  ShellPageAllocator() {
    current_bytes_allocated_ = 0;
    kPageSize = LB_PAGE_SIZE;
  }

  virtual ~ShellPageAllocator() {
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

  static ShellPageAllocator* getInstance();

  static bool instanceExists() {
    return instance_ != NULL;
  }

  size_t getCurrentBytesAllocated() const {
    return current_bytes_allocated_;
  }

  static size_t kPageSize;

 protected:

  virtual void* allocateInternal(size_t alignment, size_t size) = 0;
  virtual void freeInternal(void* p, size_t size) = 0;

  void updateAllocatedBytes(ssize_t bytes) {
    current_bytes_allocated_ += bytes;
  }

  static ShellPageAllocator* instance_;

 private:
  size_t current_bytes_allocated_;
  WTF::Mutex mutex_;

};

#if defined(LB_HAS_MMAP)
// Allocator that uses lb_mmap for page-aligned allocations.
// Uses memalign for non-page-aligned allocations.
class ShellPageAllocatorMmap : public ShellPageAllocator {
 public:
  static ShellPageAllocatorMmap* Create() {
    return new ShellPageAllocatorMmap();
  }

 protected:
  void* allocateInternal(size_t alignment, size_t size) OVERRIDE;
  void freeInternal(void* addr, size_t size) OVERRIDE;

 private:
  ShellPageAllocatorMmap();
  // Set of allocations that were malloced rather than mmapped.
  // These addresses should be freed rather than munmapped.
  std::set<void*> non_page_blocks_;
};
#endif  // defined(LB_HAS_MMAP)

// Special case allocator for managing a fixed pool of 64K-size, 64K-aligned
// blocks.  JavaScript heap is a big consumer of these.
class ShellPageAllocatorFixed : public ShellPageAllocator {
 public:
  ~ShellPageAllocatorFixed() {
    ::free(buffer_orig_);
    ::free(free_bitmap_);
  }
  static ShellPageAllocatorFixed* Create() {
    return new ShellPageAllocatorFixed();
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
    ::free(orig_ptr);
  }

  void* buffer_;
  void* buffer_orig_;
  size_t buffer_size_;
  uint32_t* free_bitmap_;
  int bitmap_size_;

  ShellPageAllocatorFixed();

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
