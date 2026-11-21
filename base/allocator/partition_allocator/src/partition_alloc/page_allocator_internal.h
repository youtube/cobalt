// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_
#define PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"

namespace partition_alloc::internal {

uintptr_t SystemAllocPages(uintptr_t hint,
                           size_t length,
                           PageAccessibilityConfiguration accessibility,
                           PageTag page_tag,
                           int file_descriptor_for_shared_alloc = -1);

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
// Accounts `length` bytes mapped (or unmapped) by a direct mmap() outside
// SystemAllocPages() (32-bit super-page regions and bottom-up mode), so that
// GetTotalMappedSize() stays complete and a later FreePages() balanced.
void NoteMappedAddressSpace(size_t length);
void NoteUnmappedAddressSpace(size_t length);
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_
