/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/analytics/memory_tracker.h"

#include "nb/analytics/memory_tracker_impl.h"
#include "nb/scoped_ptr.h"
#include "starboard/once.h"

namespace nb {
namespace analytics {
namespace {
SB_ONCE_INITIALIZE_FUNCTION(MemoryTrackerImpl, GetMemoryTrackerImplSingleton);
}  // namespace

MemoryTracker* MemoryTracker::Get() {
  MemoryTracker* t = GetMemoryTrackerImplSingleton();
  return t;
}


MemoryStats GetProcessMemoryStats() {
  MemoryStats memory_stats;

  memory_stats.total_cpu_memory = SbSystemGetTotalCPUMemory();
  memory_stats.used_cpu_memory = SbSystemGetUsedCPUMemory();

  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    int64_t used_gpu_memory = SbSystemGetUsedGPUMemory();
    memory_stats.total_gpu_memory = SbSystemGetTotalGPUMemory();
    memory_stats.used_gpu_memory = SbSystemGetUsedGPUMemory();
  }
  return memory_stats;
}

}  // namespace analytics
}  // namespace nb
