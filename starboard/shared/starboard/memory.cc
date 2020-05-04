// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/memory.h"
#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/memory_reporter.h"
#include "starboard/shared/starboard/memory_reporter_internal.h"

namespace {

inline void* SbMemoryAllocateImpl(size_t size);
inline void* SbMemoryAllocateAlignedImpl(size_t alignment, size_t size);
inline void* SbMemoryReallocateImpl(void* memory, size_t size);
inline void SbReportAllocation(const void* memory, size_t size);
inline void SbReportDeallocation(const void* memory);

SbMemoryReporter* s_memory_reporter = NULL;

bool LeakTraceEnabled();               // True when leak tracing enabled.
bool StarboardAllowsMemoryTracking();  // True when build enabled.

}  // namespace

bool SbMemorySetReporter(SbMemoryReporter* reporter) {
  // TODO: We should run a runtime test here with a test memory
  // reporter that determines whether global operator new/delete are properly
  // overridden. This problem appeared with address sanitizer and given
  // how tricky operator new/delete are in general (see a google search)
  // it's reasonable to assume that the likely hood of this happening again
  // is high. To allow the QA/developer to take corrective action, this
  // condition needs to be detected at runtime with a simple error message so
  // that corrective action (i.e. running a different build) can be applied.
  //
  // RunOperatorNewDeleteRuntimeTest();  // Implement me.

  // Flush local memory to main so that other threads don't
  // see a partially constructed reporter due to memory
  // re-ordering.
  SbAtomicMemoryBarrier();
  s_memory_reporter = reporter;

  // These are straight forward error messages. We use the build settings to
  // predict whether the MemoryReporter is likely to fail.
  if (!StarboardAllowsMemoryTracking()) {
    SbLogRaw("\nMemory Reporting is disabled because this build does "
             "not support it. Try a QA, devel or debug build.\n");
    return false;
  } else if (LeakTraceEnabled()) {
    SbLogRaw("\nMemory Reporting might be disabled because leak trace "
             "(from address sanitizer?) is active.\n");
    return false;
  }
  return true;
}

void* SbMemoryAllocate(size_t size) {
  void* memory = SbMemoryAllocateImpl(size);
  SbReportAllocation(memory, size);
  return memory;
}

void* SbMemoryAllocateNoReport(size_t size) {
  void* memory = SbMemoryAllocateImpl(size);
  return memory;
}

void* SbMemoryAllocateAligned(size_t alignment, size_t size) {
  void* memory = SbMemoryAllocateAlignedImpl(alignment, size);
  SbReportAllocation(memory, size);
  return memory;
}

void* SbMemoryReallocate(void* memory, size_t size) {
  SbReportDeallocation(memory);
  void* new_memory = SbMemoryReallocateImpl(memory, size);
  SbReportAllocation(new_memory, size);
  return new_memory;
}

void SbMemoryDeallocate(void* memory) {
  // Report must happen first or else a race condition allows the memory to
  // be freed and then reported as allocated, before the allocation is removed.
  SbReportDeallocation(memory);
  SbMemoryFree(memory);
}

void SbMemoryDeallocateNoReport(void* memory) {
  SbMemoryFree(memory);
}

void SbMemoryDeallocateAligned(void* memory) {
  // Report must happen first or else a race condition allows the memory to
  // be freed and then reported as allocated, before the allocation is removed.
  SbReportDeallocation(memory);
  SbMemoryFreeAligned(memory);
}

// Same as SbMemoryReallocateUnchecked, but will abort() in the case of an
// allocation failure
void* SbMemoryReallocateChecked(void* memory, size_t size) {
  void* address = SbMemoryReallocateUnchecked(memory, size);
  SbAbortIfAllocationFailed(size, address);
  return address;
}

// Same as SbMemoryAllocateAlignedUnchecked, but will abort() in the case of an
// allocation failure
void* SbMemoryAllocateAlignedChecked(size_t alignment, size_t size) {
  void* address = SbMemoryAllocateAlignedUnchecked(alignment, size);
  SbAbortIfAllocationFailed(size, address);
  return address;
}

void* SbMemoryAllocateChecked(size_t size) {
  void* address = SbMemoryAllocateUnchecked(size);
  SbAbortIfAllocationFailed(size, address);
  return address;
}

void SbMemoryReporterReportMappedMemory(const void* memory, size_t size) {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter)) {
    return;
  }
  s_memory_reporter->on_mapmem_cb(
      s_memory_reporter->context,
      memory,
      size);
#endif  // STARBOARD_ALLOWS_MEMORY_TRACKING
}

void SbMemoryReporterReportUnmappedMemory(const void* memory, size_t size) {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter)) {
    return;
  }
  s_memory_reporter->on_unmapmem_cb(
      s_memory_reporter->context,
      memory,
      size);
#endif  // STARBOARD_ALLOWS_MEMORY_TRACKING
}

namespace {

inline void SbReportAllocation(const void* memory, size_t size) {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter)) {
    return;
  }
  s_memory_reporter->on_alloc_cb(
      s_memory_reporter->context,
      memory,
      size);
#endif  // STARBOARD_ALLOWS_MEMORY_TRACKING
}

inline void SbReportDeallocation(const void* memory) {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter)) {
    return;
  }
  s_memory_reporter->on_dealloc_cb(
      s_memory_reporter->context,
      memory);
#endif  // STARBOARD_ALLOWS_MEMORY_TRACKING
}

inline void* SbMemoryAllocateImpl(size_t size) {
#if SB_ABORT_ON_ALLOCATION_FAILURE
  return SbMemoryAllocateChecked(size);
#else
  return SbMemoryAllocateUnchecked(size);
#endif
}

inline void* SbMemoryAllocateAlignedImpl(size_t alignment, size_t size) {
#if SB_ABORT_ON_ALLOCATION_FAILURE
  return SbMemoryAllocateAlignedChecked(alignment, size);
#else
  return SbMemoryAllocateAlignedUnchecked(alignment, size);
#endif
}

inline void* SbMemoryReallocateImpl(void* memory, size_t size) {
#if SB_ABORT_ON_ALLOCATION_FAILURE
  return SbMemoryReallocateChecked(memory, size);
#else
  return SbMemoryReallocateUnchecked(memory, size);
#endif
}

bool LeakTraceEnabled() {
#if defined(HAS_LEAK_SANITIZER) && (0 == HAS_LEAK_SANITIZER)
  // In this build the leak tracer is specifically disabled. This
  // build condition is typical for some builds of address sanitizer.
  return true;
#elif defined(ADDRESS_SANITIZER)
  // Leak tracer is not specifically disabled and address sanitizer is running.
  return true;
#else
  return false;
#endif
}

bool StarboardAllowsMemoryTracking() {
#if defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return true;
#else
  return false;
#endif
}

}  // namespace
