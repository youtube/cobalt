// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_oom.h"

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "build/build_config.h"

namespace partition_alloc::internal {

OomFunction g_oom_handling_function = nullptr;

PA_NOINLINE PA_NOT_TAIL_CALLED void PartitionExcessiveAllocationSize(
    size_t size) {
  PA_NO_CODE_FOLDING();
  OOM_CRASH(size);
}

#if !defined(ARCH_CPU_64_BITS)
PA_NOINLINE PA_NOT_TAIL_CALLED void
PartitionOutOfMemoryWithLotsOfUncommitedPages(size_t size) {
  PA_NO_CODE_FOLDING();
  OOM_CRASH(size);
}

[[noreturn]] PA_NOT_TAIL_CALLED PA_NOINLINE void
PartitionOutOfMemoryWithLargeVirtualSize(size_t virtual_size) {
  PA_NO_CODE_FOLDING();
  OOM_CRASH(virtual_size);
}

#endif  // !defined(ARCH_CPU_64_BITS)

}  // namespace partition_alloc::internal
