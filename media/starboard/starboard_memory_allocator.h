// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_STARBOARD_MEMORY_ALLOCATOR_H_
#define MEDIA_STARBOARD_STARBOARD_MEMORY_ALLOCATOR_H_

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "media/starboard/media_pool_direct_map.h"
#include "starboard/common/allocator.h"
#include "starboard/common/memory.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"

namespace media {

using starboard::AlignDown;
using starboard::AlignUp;
using starboard::Allocator;
using DecommitMode = starboard::Allocator::DecommitMode;

// StarboardMemoryAllocator is an allocator that allocates and frees memory
// using posix_memalign() and free(), or with kCobaltMediaPoolSoftFail on 32-bit
// builds, MediaPoolDirectMap.
class StarboardMemoryAllocator : public starboard::Allocator {
 public:
  // With kCobaltMediaPoolSoftFail, a request smaller than 2 MiB may be served
  // by a larger mid-sized block (see AllocateForAlignment()), but never by one
  // larger than `max_step_size`; 0 means always the requested size.
  StarboardMemoryAllocator(bool enable_decommit,
                           bool enable_page_alignment,
                           size_t max_step_size = 0)
      : enable_decommit_(enable_decommit),
        enable_page_alignment_(enable_page_alignment),
        max_step_size_(max_step_size),
        page_size_(static_cast<size_t>(sysconf(_SC_PAGESIZE))) {
    LOG(INFO) << "StarboardMemoryAllocator: decommit is "
              << (enable_decommit_ ? "enabled" : "disabled")
              << ", page alignment is "
              << (enable_page_alignment_ ? "enabled" : "disabled") << ".";
  }

  void* Allocate(size_t size) override {
    return AllocateForAlignment(&size, 1);
  }

  void* Allocate(size_t size, size_t alignment) override {
    return AllocateForAlignment(&size, alignment);
  }

  void* AllocateForAlignment(size_t* size, size_t alignment) override {
    if (enable_page_alignment_) {
      alignment = std::max(alignment, page_size_);
      *size = AlignUp(*size, page_size_);
    }
    // kCobaltMediaPoolSoftFail: every block, whatever its size, comes from
    // page-aligned mmap(), and a failed mmap() returns nullptr instead of
    // falling back to posix_memalign(). On 32-bit, PartitionAlloc can't fail
    // softly here: a small block may need a new 2 MiB-aligned super page and a
    // large one a 2 MiB-aligned direct map, and either failure crashes.
    // EmbeddedMetadataReuseAllocatorBase::ExpandToFit() retries a failed
    // expansion with the size of the allocation that triggered it; if that
    // fails too, the caller sees an allocation failure.
    if (alignment <= page_size_ && MediaPoolDirectMap::IsEnabled()) {
      if (*size == 0) {
        return nullptr;
      }
      // Any request smaller than 2 MiB (in practice, a retry after a failed
      // expansion by the pool's increment, which asks for one frame's size)
      // prefers a mid-sized block if a hole that size exists, so that a
      // degraded pool doesn't become one guarded mapping per frame. Steps
      // never exceed `max_step_size_`, which the media pool sets to its
      // allocation increment. The pool has no maximum capacity
      // (BidirectionalFitReuseAllocator passes 0), so a step can't push it
      // past one. Over-aligned requests (alignment > page) keep
      // posix_memalign().
      for (size_t step :
           {size_t{2} << 20, size_t{1} << 20, size_t{512} << 10}) {
        if (step > *size && step <= max_step_size_) {
          size_t step_size = step;
          if (void* block = MediaPoolDirectMap::Map(&step_size)) {
            *size = step_size;
            return block;
          }
        }
      }
      return MediaPoolDirectMap::Map(size);
    }
    void* p = nullptr;
    std::ignore = posix_memalign(&p, std::max(alignment, sizeof(void*)), *size);
    return p;
  }

  bool CanAllocateContiguously() const override {
    // MediaPoolDirectMap blocks have a guard page at each end.
    return !MediaPoolDirectMap::IsEnabled();
  }

  void Free(void* memory) override {
    if (MediaPoolDirectMap::Unmap(memory)) {
      return;
    }
    free(memory);
  }

  void Decommit(void* memory, size_t size, DecommitMode mode) override {
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    CHECK(enable_decommit_);
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

    uint8_t* start = static_cast<uint8_t*>(memory);
    uint8_t* end = start + size;
    uint8_t* aligned_start = AlignUp(start, page_size_);
    uint8_t* aligned_end = AlignDown(end, page_size_);

    if (aligned_start < aligned_end) {
      size_t aligned_size = aligned_end - aligned_start;

      if (mode == DecommitMode::kCold) {
#if defined(MADV_COLD)
        madvise(aligned_start, aligned_size, MADV_COLD);
#endif  // defined(MADV_COLD)
        return;
      }

      // MADV_FREE is not supported on all kernel versions/configurations. If it
      // fails, fallback to MADV_DONTNEED to ensure memory is still decommitted.
      if (mode == DecommitMode::kConservative &&
          madvise(aligned_start, aligned_size, MADV_FREE) == 0) {
        return;
      }

      madvise(aligned_start, aligned_size, MADV_DONTNEED);
    }
  }

  size_t GetCapacity() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  size_t GetAllocated() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override {}

 private:
  const bool enable_decommit_;
  const bool enable_page_alignment_;
  const size_t max_step_size_;
  const size_t page_size_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_MEMORY_ALLOCATOR_H_
