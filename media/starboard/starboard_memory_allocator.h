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
// using posix_memalign() and free().
class StarboardMemoryAllocator : public starboard::Allocator {
 public:
  StarboardMemoryAllocator(bool enable_decommit, bool enable_page_alignment)
      : enable_decommit_(enable_decommit),
        enable_page_alignment_(enable_page_alignment),
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
    void* p = nullptr;
    std::ignore = posix_memalign(&p, std::max(alignment, sizeof(void*)), *size);
    return p;
  }
  void Free(void* memory) override { free(memory); }

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
  const size_t page_size_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_MEMORY_ALLOCATOR_H_
