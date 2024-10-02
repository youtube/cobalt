// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8_platform_page_allocator.h"

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/random.h"
#include "base/check_op.h"
#include "base/cpu.h"
#include "base/memory/page_size.h"
#include "build/build_config.h"

namespace {

// Maps the v8 page permissions into a page configuration from base.
::partition_alloc::PageAccessibilityConfiguration::Permissions
GetPagePermissions(v8::PageAllocator::Permission permission) {
  switch (permission) {
    case v8::PageAllocator::Permission::kRead:
      return ::partition_alloc::PageAccessibilityConfiguration::kRead;
    case v8::PageAllocator::Permission::kReadWrite:
      return ::partition_alloc::PageAccessibilityConfiguration::kReadWrite;
    case v8::PageAllocator::Permission::kReadWriteExecute:
      // at the moment bti-protection is not enabled for this path since some
      // projects may still be using non-bti compliant code.
      return ::partition_alloc::PageAccessibilityConfiguration::
          kReadWriteExecute;
    case v8::PageAllocator::Permission::kReadExecute:
#if defined(__ARM_FEATURE_BTI_DEFAULT)
      return base::CPU::GetInstanceNoAllocation().has_bti()
                 ? ::partition_alloc::PageAccessibilityConfiguration::
                       kReadExecuteProtected
                 : ::partition_alloc::PageAccessibilityConfiguration::
                       kReadExecute;
#else
      return ::partition_alloc::PageAccessibilityConfiguration::kReadExecute;
#endif
    case v8::PageAllocator::Permission::kNoAccessWillJitLater:
      // We could use this information to conditionally set the MAP_JIT flag
      // on Mac-arm64; however this permissions value is intended to be a
      // short-term solution, so we continue to set MAP_JIT for all V8 pages
      // for now.
      return ::partition_alloc::PageAccessibilityConfiguration::kInaccessible;
    default:
      DCHECK_EQ(v8::PageAllocator::Permission::kNoAccess, permission);
      return ::partition_alloc::PageAccessibilityConfiguration::kInaccessible;
  }
}

::partition_alloc::PageAccessibilityConfiguration GetPageConfig(
    v8::PageAllocator::Permission permission) {
  return ::partition_alloc::PageAccessibilityConfiguration(
      GetPagePermissions(permission));
}

}  // namespace

namespace gin {
PageAllocator::~PageAllocator() = default;

size_t PageAllocator::AllocatePageSize() {
  return partition_alloc::internal::PageAllocationGranularity();
}

size_t PageAllocator::CommitPageSize() {
  return base::GetPageSize();
}

void PageAllocator::SetRandomMmapSeed(int64_t seed) {
  ::partition_alloc::SetMmapSeedForTesting(seed);
}

void* PageAllocator::GetRandomMmapAddr() {
  return reinterpret_cast<void*>(::partition_alloc::GetRandomPageBase());
}

void* PageAllocator::AllocatePages(void* address,
                                   size_t length,
                                   size_t alignment,
                                   v8::PageAllocator::Permission permissions) {
  partition_alloc::PageAccessibilityConfiguration config =
      GetPageConfig(permissions);
  return partition_alloc::AllocPages(address, length, alignment, config,
                                     partition_alloc::PageTag::kV8);
}

bool PageAllocator::FreePages(void* address, size_t length) {
  partition_alloc::FreePages(address, length);
  return true;
}

bool PageAllocator::ReleasePages(void* address,
                                 size_t length,
                                 size_t new_length) {
  DCHECK_LT(new_length, length);
  uint8_t* release_base = reinterpret_cast<uint8_t*>(address) + new_length;
  size_t release_size = length - new_length;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On POSIX, we can unmap the trailing pages.
  partition_alloc::FreePages(release_base, release_size);
#elif BUILDFLAG(IS_WIN)
  // On Windows, we can only de-commit the trailing pages. FreePages() will
  // still free all pages in the region including the released tail, so it's
  // safe to just decommit the tail.
  partition_alloc::DecommitSystemPages(
      release_base, release_size,
      ::partition_alloc::PageAccessibilityDisposition::kRequireUpdate);
#else
#error Unsupported platform
#endif
  return true;
}

bool PageAllocator::SetPermissions(void* address,
                                   size_t length,
                                   Permission permissions) {
  // If V8 sets permissions to none, we can discard the memory.
  if (permissions == v8::PageAllocator::Permission::kNoAccess) {
    // Use PageAccessibilityDisposition::kAllowKeepForPerf as an
    // optimization, to avoid perf regression (see crrev.com/c/2563038 for
    // details). This may cause the memory region to still be accessible on
    // certain platforms, but at least the physical pages will be discarded.
    partition_alloc::DecommitSystemPages(
        address, length,
        ::partition_alloc::PageAccessibilityDisposition::kAllowKeepForPerf);
    return true;
  } else {
    return partition_alloc::TrySetSystemPagesAccess(address, length,
                                                    GetPageConfig(permissions));
  }
}

bool PageAllocator::RecommitPages(void* address,
                                  size_t length,
                                  Permission permissions) {
  partition_alloc::RecommitSystemPages(
      reinterpret_cast<uintptr_t>(address), length, GetPageConfig(permissions),
      partition_alloc::PageAccessibilityDisposition::kAllowKeepForPerf);
  return true;
}

bool PageAllocator::DiscardSystemPages(void* address, size_t size) {
  partition_alloc::DiscardSystemPages(address, size);
  return true;
}

bool PageAllocator::DecommitPages(void* address, size_t size) {
  // V8 expects the pages to be inaccessible and zero-initialized upon next
  // access.
  partition_alloc::DecommitAndZeroSystemPages(address, size);
  return true;
}

partition_alloc::PageAccessibilityConfiguration::Permissions
PageAllocator::GetPageConfigPermissionsForTesting(
    v8::PageAllocator::Permission permission) {
  return GetPageConfig(permission).permissions;
}

}  // namespace gin
