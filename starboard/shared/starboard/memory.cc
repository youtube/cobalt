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

  // Memory reporting is removed
  return false;
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
#if !defined(COBALT_BUILD_TYPE_GOLD)
  SB_CHECK((size != 0) || (memory == nullptr))
      << "Calling SbMemoryReallocate with a non-null pointer and size 0 is not "
         "guaranteed to release the memory, and therefore may leak memory.";
#endif
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
  return;
}

void SbMemoryReporterReportUnmappedMemory(const void* memory, size_t size) {
  return;
}

namespace {

inline void SbReportAllocation(const void* memory, size_t size) {
  return;
}

inline void SbReportDeallocation(const void* memory) {
  return;
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

}  // namespace
