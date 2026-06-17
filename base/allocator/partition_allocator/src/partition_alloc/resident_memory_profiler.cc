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

#include "base/allocator/partition_allocator/src/partition_alloc/resident_memory_profiler.h"


#include "build/build_config.h"
#if BUILDFLAG(IS_COBALT) && PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#include <atomic>
#include <random>

#include "base/allocator/partition_allocator/src/partition_alloc/in_slot_metadata.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/allocator/partition_allocator/src/partition_alloc/internal_allocator.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_lock.h"

namespace partition_alloc {

namespace {


struct SampledMetadata {
  size_t allocation_size;
  double resident_weight;
  // Stack trace could be added here later if requested.
};

using SampledAllocationsMap = std::unordered_map<
    void*,
    SampledMetadata,
    std::hash<void*>,
    std::equal_to<void*>,
    internal::InternalAllocator<std::pair<void* const, SampledMetadata>>>;

constexpr size_t kNumShards = 16;
SampledAllocationsMap* g_sampled_allocations[kNumShards] = {};
internal::Lock g_shard_locks[kNumShards];

std::atomic<size_t> resident_counters_[kNumShards] = {};

inline size_t GetShard(void* address) {
  return (reinterpret_cast<uintptr_t>(address) >> 4) % kNumShards;
}

}  // namespace

std::atomic<bool> g_is_memory_profiler_enabled{false};
std::atomic<double> g_memory_profiler_target_percent{0.01};

PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool IsMemoryProfilerSamplingEnabled() {
  return g_is_memory_profiler_enabled.load(std::memory_order_relaxed);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC) ThreadLocalData* GetThreadLocalData() {
  thread_local ThreadLocalData tld;
  return &tld;
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void SampleAllocation(void* address, size_t size, ThreadLocalData* tld) {
  if (!IsMemoryProfilerSamplingEnabled()) {
    // Prevent repeated sampling if feature is disabled.
    tld->bytes_until_next_sample = 1024 * 1024 * 1024; 
    return;
  }

  // Base sample interval is ~100MB to achieve a ~1% sampling rate depending on config
  // The probability is `size / interval`.
  double target_percent = g_memory_profiler_target_percent.load(std::memory_order_relaxed);
  if (target_percent <= 0.0 || target_percent > 100.0) {
    target_percent = 0.01;
  }
  
  // Poisson sampling logic
  double interval = 100.0 * 1024 * 1024 / target_percent; 

  // Reset counter using Poisson distribution
  thread_local std::mt19937 generator(reinterpret_cast<uintptr_t>(address));
  std::exponential_distribution<double> distribution(1.0 / interval);
  tld->bytes_until_next_sample = static_cast<int64_t>(distribution(generator));
  if (tld->bytes_until_next_sample <= 0) {
    tld->bytes_until_next_sample = 1;
  }

  double weight = interval; 

  size_t shard = GetShard(address);
  {
    internal::ScopedGuard lock(g_shard_locks[shard]);
    if (!g_sampled_allocations[shard]) {
      g_sampled_allocations[shard] = internal::ConstructAtInternalPartition<SampledAllocationsMap>();
    }
    (*g_sampled_allocations[shard])[address] = {size, weight};
    resident_counters_[shard].fetch_add(size, std::memory_order_relaxed);
  }

  auto* metadata = PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
      internal::SlotStart::FromObject(address).untagged_slot_start_,
      internal::ReadOnlySlotSpanMetadata::FromObject(address)->bucket->slot_size);
  metadata->SetSampled();
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC) void OnFreeSampled(void* address) {

  auto* metadata = PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
      internal::SlotStart::FromObject(address).untagged_slot_start_,
      internal::ReadOnlySlotSpanMetadata::FromObject(address)->bucket->slot_size);

  metadata->ClearSampled();

  size_t shard = GetShard(address);
  size_t size = 0;
  {
    internal::ScopedGuard lock(g_shard_locks[shard]);
    if (g_sampled_allocations[shard]) {
      auto it = g_sampled_allocations[shard]->find(address);
      if (it != g_sampled_allocations[shard]->end()) {
        size = it->second.allocation_size;
        g_sampled_allocations[shard]->erase(it);
      }
    }
  }

  if (size > 0) {
    resident_counters_[shard].fetch_sub(size, std::memory_order_relaxed);
  }
}
PA_COMPONENT_EXPORT(PARTITION_ALLOC) size_t GetTotalSampledResidentMemory() {
  size_t total = 0;
  for (size_t i = 0; i < kNumShards; ++i) {
    total += resident_counters_[i].load(std::memory_order_relaxed);
  }
  return total;
}

}  // namespace partition_alloc

#endif  // BUILDFLAG(IS_COBALT) && PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
