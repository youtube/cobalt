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

// Adapted from OSAllocatorShell.cpp

#include "config.h"
#include "OSAllocatorStarboard.h"

#include "starboard/memory.h"

namespace {

uint8_t BitScanForward(uint32_t *index, uint32_t mask) {
    *index = 0;
    while (!(mask & 1)) {
      mask >>= 1;
      ++(*index);
    }
    return !!(mask & 1);
}

// Find the least-significant bit set.
int FindFirstSet(uint32_t i) {
    uint32_t index;
    if (BitScanForward(&index, i)) {
      // ffs returns values from 1 - 32
      return index + 1;
    } else {
      return 0;
    }
}

}  // namespace

namespace WTF {

// ===== on allocating =====

// Allocations must be aligned to >= getpagesize().
// getpagesize() must be <= 64k for PageAllocationAligned.
// getpagesize() must be <= JSC::RegisterFile::commitSize (default 16k).
// To minimize waste in PageAllocationAligned, getpagesize() must be 64k
// and commitSize must be increased to 64k.


// Define this when performing leak analysis.  It will avoid pooling memory.
//#define LEAK_ANALYSIS

// static
StarboardPageAllocator* StarboardPageAllocator::instance_ = NULL;
size_t StarboardPageAllocator::kPageSize;

#if SB_HAS(MMAP)
StarboardPageAllocatorMmap::StarboardPageAllocatorMmap() {}

void* StarboardPageAllocatorMmap::allocateInternal(size_t alignment, size_t size) {
    void* out = NULL;
    if (alignment == kPageSize) {
        // TODO: compute and track wasted space here.
        // (Basically just the diff between allocated_bytes
        // and page_count * page_size)
        void* mapped = SbMemoryMap(size, kSbMemoryMapProtectReadWrite, "WTFStarboardPageAllocator");
        if (mapped != SB_MEMORY_MAP_FAILED) {
            ASSERT(reinterpret_cast<uintptr_t>(mapped) % kPageSize == 0);
            updateAllocatedPages((size + kPageSize - 1) / kPageSize);
            out = mapped;
        }
    } else {
        ASSERT(alignment <= 16);
        out = SbMemoryAllocateAligned(alignment, size);
        if (out != NULL) {
            non_page_blocks_.insert(out);
        }
    }
    if (out) {
        updateAllocatedBytes(size);
    }
    return out;
}

void StarboardPageAllocatorMmap::freeInternal(void* addr, size_t size) {
    updateAllocatedBytes(-static_cast<ssize_t>(size));
    std::set<void*>::iterator it = non_page_blocks_.find(addr);
    if (it == non_page_blocks_.end()) {
        ssize_t page_count = (
            static_cast<ssize_t>(size) + kPageSize - 1) / kPageSize;
        updateAllocatedPages(-page_count);
        SbMemoryUnmap(addr, size);
    } else {
        SbMemoryFreeAligned(addr);
        non_page_blocks_.erase(it);
    }
}
#endif  // SB_HAS(MMAP)

StarboardPageAllocatorFixed::StarboardPageAllocatorFixed()
#if !defined(__LB_SHELL__FOR_RELEASE__) && defined(MEMORY_STATISTICS)
    : excess_page_count_(
        "WebKit.StarboardPageAllocator.ExcessPageCount", 0,
        "Number of requested pages that the StarboardPageAllocator could not "
        "fulfill.")
    , excess_high_water_mark_(
        "WebKit.StarboardPageAllocator.ExcessHighWaterMark", 0,
        "Most excess pages at any time during the app's current lifetime.")
#endif
{
#if defined(LEAK_ANALYSIS)
    static const int kBufferSize = 64 * 1024;
#else
    static const int kBufferSize = 19 * 1024 * 1024;
#endif

    buffer_size_ = kBufferSize;
    const int page_count = align(buffer_size_, kPageSize) / kPageSize;
    bitmap_size_ = align(page_count, 32) / 32;

    buffer_orig_ = SbMemoryAllocate(buffer_size_ + kPageSize);
    buffer_ = alignPtr(buffer_orig_, kPageSize);

    ASSERT(buffer_ != NULL);
    free_bitmap_ = (uint32_t*)SbMemoryAllocate(bitmap_size_ * sizeof(uint32_t));
    ASSERT(free_bitmap_);

    for (int i = 0; i < bitmap_size_; ++i) {
        free_bitmap_[i] = 0xffffffff;
    }

    // If number of pages doesn't fit neatly into bitmap, mark any trailing
    // extra bits as allocated so we never return them.
    const int trailing_bits = (32 - (page_count % 32)) % 32;
    for (int i = 32 - trailing_bits; i < 32; ++i) {
        clear_bit(bitmap_size_ - 1, i);
    }
}

void* StarboardPageAllocatorFixed::allocateInternal(
    size_t alignment, size_t size) {
    void* p = 0;
    if (size == kPageSize) {
        p = allocatePage();
    }
    if (!p) {
        p = allocateBlock(alignment, size);
    }
    if (p) {
        updateAllocatedBytes(size);
    }
    return p;
}

void StarboardPageAllocatorFixed::freeInternal(void* addr, size_t size) {
    if (isValidPage(addr)) {
        freePage(addr);
    } else {
        freeBlock(addr);
    }
    updateAllocatedBytes(-static_cast<ssize_t>(size));
}

void* StarboardPageAllocatorFixed::allocatePage() {
    void* p = NULL;
    for (int i = 0; i < bitmap_size_; ++i) {
        if (free_bitmap_[i] != 0) {
            // index of the first bit set in this integer.
            int first_free = FindFirstSet(free_bitmap_[i]) - 1;
            int page_index = i * 32 + first_free;
            p = (void*) (uintptr_t(buffer_) + kPageSize * page_index);
            clear_bit(i, first_free);
#if !defined(__LB_SHELL__FOR_RELEASE__) && defined(MEMORY_STATISTICS)
            excess_page_count_ = 0;
#endif
            break;
        }
    }
#if !defined(__LB_SHELL__FOR_RELEASE__) && defined(MEMORY_STATISTICS)
    if (!p) {
        excess_page_count_ += 1;
        excess_high_water_mark_ =
                std::max(excess_high_water_mark_, excess_page_count_);
    }
#endif
    return p;
}

void StarboardPageAllocatorFixed::freePage(void* p) {
    uintptr_t address = (uintptr_t)p;
    ASSERT(address % kPageSize == 0);
    uintptr_t start = (uintptr_t)buffer_;
    uint32_t offset = (address - start) / kPageSize;
    uint32_t int_index = offset / 32;
    uint32_t bit_index = offset % 32;
    set_bit(int_index, bit_index);
}

// A valid page is one that was allocated from buffer_.
bool StarboardPageAllocatorFixed::isValidPage(void* p) const {
    uintptr_t address = (uintptr_t)p;
    uintptr_t start = (uintptr_t)buffer_;
    if (address >= start && address < start + buffer_size_) {
        return true;
    } else {
        return false;
    }
}

void* StarboardPageAllocatorFixed::allocateBlock(size_t alignment, size_t size) {
    // Overallocate so we can guarantee an aligned block.  We do not use
    // memalign() here because some platforms do not support memalign() with
    // alignment greater than 16 bytes (e.g. 64kb).  We write the address of the
    // original pointer just behind the block to be returned.
    void* ptr = SbMemoryAllocate(size + alignment + sizeof(void*));
    if (!ptr) {
        return NULL;
    }

    void* aligned_ptr = alignPtr(
        (void*)((uintptr_t)ptr + sizeof(void*)), alignment);

    // Check that we have room to write the header
    ASSERT((uintptr_t)aligned_ptr - (uintptr_t)ptr >= sizeof(void*));
    void** orig_ptr = (void**)((uintptr_t)aligned_ptr - sizeof(void*));
    *orig_ptr = ptr;
    return aligned_ptr;
}

StarboardPageAllocator* StarboardPageAllocator::getInstance() {
    if (!instance_) {
#if SB_HAS(MMAP)
        instance_ = StarboardPageAllocatorMmap::Create();
#else
        instance_ = StarboardPageAllocatorFixed::Create();
#endif
    }

    return instance_;
}

void* OSAllocator::reserveUncommitted(size_t vm_size, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    StarboardPageAllocator* allocator = StarboardPageAllocator::getInstance();
    const size_t alignment = usage == JSUnalignedPages ?
                             16 : StarboardPageAllocator::kPageSize;
    return allocator->allocate(alignment, vm_size);
}

// static
void OSAllocator::releaseDecommitted(void* addr, size_t size)
{
    StarboardPageAllocator* allocator = StarboardPageAllocator::getInstance();
    allocator->free(addr, size);
}

// static
void OSAllocator::commit(void* addr, size_t size, bool writable, bool executable)
{
    // no commit/decommit scheme on these platforms
}

// static
void OSAllocator::decommit(void* addr, size_t size)
{
    // no commit/decommit scheme on these platforms
}

// static
void* OSAllocator::reserveAndCommit(size_t size, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    // no commit/decommit scheme on these platforms
    return reserveUncommitted(size, usage, writable, executable, includesGuardPages);
}

// static
size_t OSAllocator::getCurrentBytesAllocated() {
#if !defined(__LB_SHELL__FOR_RELEASE__) && defined(MEMORY_STATISTICS)
    if (StarboardPageAllocator::instanceExists()) {
        return StarboardPageAllocator::getInstance()->getCurrentBytesAllocated();
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

} // namespace WTF
