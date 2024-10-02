// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_partitionalloc_shims.h"

#include <algorithm>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/lightweight_detector.h"

namespace gwp_asan {
namespace internal {

namespace {

SamplingState<PARTITIONALLOC> sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

bool AllocationHook(void** out,
                    unsigned int flags,
                    size_t size,
                    const char* type_name) {
  if (UNLIKELY(sampling_state.Sample())) {
    // Ignore allocation requests with unknown flags.
    constexpr int kKnownFlags = partition_alloc::AllocFlags::kReturnNull |
                                partition_alloc::AllocFlags::kZeroFill;
    if (flags & ~kKnownFlags)
      return false;

    if (void* allocation = gpa->Allocate(size, 0, type_name)) {
      *out = allocation;
      return true;
    }
  }
  return false;
}

bool FreeHook(void* address) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    gpa->Deallocate(address);
    return true;
  }
  return false;
}

bool ReallocHook(size_t* out, void* address) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    *out = gpa->GetRequestedSize(address);
    return true;
  }
  return false;
}

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetPartitionAllocGpaForTesting() {
  return *gpa;
}

void QuarantineHook(void* address, size_t size) {
  gpa->RecordLightweightDeallocation(address, size);
}

void InstallPartitionAllocHooks(
    size_t max_allocated_pages,
    size_t num_metadata,
    size_t total_pages,
    size_t sampling_frequency,
    GuardedPageAllocator::OutOfMemoryCallback callback,
    LightweightDetector::State lightweight_detector_state,
    size_t num_lightweight_detector_metadata) {
  static crash_reporter::CrashKeyString<24> pa_crash_key(
      kPartitionAllocCrashKey);
  gpa = new GuardedPageAllocator();
  gpa->Init(max_allocated_pages, num_metadata, total_pages, std::move(callback),
            true, lightweight_detector_state,
            num_lightweight_detector_metadata);
  pa_crash_key.Set(gpa->GetCrashKey());
  sampling_state.Init(sampling_frequency);
  // TODO(vtsyrklevich): Allow SetOverrideHooks to be passed in so we can hook
  // PDFium's PartitionAlloc fork.
  partition_alloc::PartitionAllocHooks::SetOverrideHooks(
      &AllocationHook, &FreeHook, &ReallocHook);
  if (lightweight_detector_state == LightweightDetector::State::kEnabled) {
    partition_alloc::PartitionAllocHooks::SetQuarantineOverrideHook(
        &QuarantineHook);
  }
}

}  // namespace internal
}  // namespace gwp_asan
