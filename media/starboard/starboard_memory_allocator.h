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

using starboard::common::AlignDown;
using starboard::common::AlignUp;

// StarboardMemoryAllocator is an allocator that allocates and frees memory
// using posix_memalign() and free().
class StarboardMemoryAllocator : public starboard::common::Allocator {
 public:
  explicit StarboardMemoryAllocator(bool enable_decommit)
      : enable_decommit_(enable_decommit),
        page_size_(static_cast<size_t>(sysconf(_SC_PAGESIZE))) {
    LOG(INFO) << "StarboardMemoryAllocator: decommit is "
              << (enable_decommit_ ? "enabled" : "disabled") << ".";
  }

  void* Allocate(std::size_t size) override {
    return AllocateForAlignment(&size, 1);
  }

  void* Allocate(std::size_t size, std::size_t alignment) override {
    return AllocateForAlignment(&size, alignment);
  }

  void* AllocateForAlignment(std::size_t* size,
                             std::size_t alignment) override {
    if (enable_decommit_) {
      alignment = std::max(alignment, page_size_);
      *size = AlignUp(*size, page_size_);
    }
    void* p = nullptr;
    posix_memalign(&p, std::max(alignment, sizeof(void*)), *size);
    return p;
  }
  void Free(void* memory) override { free(memory); }

  void Decommit(void* memory, std::size_t size) override {
    uintptr_t start = reinterpret_cast<uintptr_t>(memory);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    CHECK(enable_decommit_);
    CHECK_EQ(start % page_size_, 0U);
    CHECK_EQ(size % page_size_, 0U);
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

    // Align down the size to prevent decommitting past the end just in case
    // the user explicitly passes an altered, unaligned size block.
    size_t aligned_size = AlignDown(size, page_size_);

    if (aligned_size > 0) {
      madvise(memory, aligned_size, MADV_DONTNEED);
    }
  }

  std::size_t GetCapacity() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  std::size_t GetAllocated() const override {
    // Returns 0 here to avoid tracking the allocated memory.
    return 0;
  }
  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override {}

 private:
  const bool enable_decommit_;
  const size_t page_size_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_MEMORY_ALLOCATOR_H_
