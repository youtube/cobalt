// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_PAGE_SIZE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_PAGE_SIZE_H_

#include <stddef.h>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base {

// Returns the number of bytes in a memory page. Do not use this to compute
// the number of pages in a block of memory for calling mincore(). On some
// platforms, e.g. iOS, mincore() uses a different page size from what is
// returned by GetPageSize().
PA_COMPONENT_EXPORT(PARTITION_ALLOC) size_t GetPageSize();

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_MEMORY_PAGE_SIZE_H_
