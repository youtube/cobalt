// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_RESIDENT_MEMORY_PROFILER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_RESIDENT_MEMORY_PROFILER_H_


#include "build/build_config.h"
#include "build/buildflag.h" // nogncheck
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(IS_COBALT) && PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#include <stddef.h>
#include <stdint.h>
#include <atomic>

#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc {

PA_COMPONENT_EXPORT(PARTITION_ALLOC) extern std::atomic<bool> g_is_memory_profiler_enabled;
PA_COMPONENT_EXPORT(PARTITION_ALLOC) extern std::atomic<double> g_memory_profiler_target_percent;

struct PA_COMPONENT_EXPORT(PARTITION_ALLOC) ThreadLocalData {
  int64_t bytes_until_next_sample = 0;
};

// Returns whether the profiler is enabled.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsMemoryProfilerSamplingEnabled();

// Returns the thread-local state used by the memory profiler.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) ThreadLocalData* GetThreadLocalData();

// Called from the allocation fast path when bytes_until_next_sample <= 0.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void SampleAllocation(void* address, size_t size, ThreadLocalData* tld);

// Called from the deallocation fast path when kSampledBitMask is set.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void OnFreeSampled(void* address);

// Returns the total sampled resident memory in bytes.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) size_t GetTotalSampledResidentMemory();

}  // namespace partition_alloc

#endif  // BUILDFLAG(IS_COBALT) && PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_RESIDENT_MEMORY_PROFILER_H_
