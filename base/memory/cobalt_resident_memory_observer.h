// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BASE_MEMORY_COBALT_RESIDENT_MEMORY_OBSERVER_H_
#define BASE_MEMORY_COBALT_RESIDENT_MEMORY_OBSERVER_H_

#include <atomic>
#include <cstdint>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/memory/cobalt_memory_context.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace cobalt {
namespace memory {
class CobaltMemoryAttributionManagerTest;
}
}

namespace base {
namespace memory {

// Observer for PartitionAlloc allocations via base::PoissonAllocationSampler.
// It tracks statistically estimated net resident memory footprint per MemoryContext
// without modifying PartitionAlloc's fast-paths or in-slot metadata.
class BASE_EXPORT CobaltResidentMemoryObserver : public base::PoissonAllocationSampler::SamplesObserver {
 public:
  friend class cobalt::memory::CobaltMemoryAttributionManagerTest;

  struct AlignedCounter {
    // Align to 64 bytes (standard cache line) to eliminate false sharing.
    alignas(64) std::atomic<uint64_t> value{0};
  };

  static CobaltResidentMemoryObserver* Get();

  CobaltResidentMemoryObserver(const CobaltResidentMemoryObserver&) = delete;
  CobaltResidentMemoryObserver& operator=(const CobaltResidentMemoryObserver&) = delete;

  void Start();
  void Stop();

  base::span<AlignedCounter> GetCounters() {
    return base::span<AlignedCounter>(counters_);
  }

 private:
  friend class base::NoDestructor<CobaltResidentMemoryObserver>;
  CobaltResidentMemoryObserver();
  ~CobaltResidentMemoryObserver() override = default;

 public:
  // base::PoissonAllocationSampler::SamplesObserver
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::allocator::dispatcher::AllocationSubsystem type,
                   const char* context) override;
  void SampleRemoved(void* address) override;

 private:

  struct SampleData {
    uint8_t memory_context;
    size_t total;
  };

  base::Lock mutex_;
  absl::flat_hash_map<void*, SampleData> live_samples_ GUARDED_BY(mutex_);

  AlignedCounter counters_[static_cast<size_t>(MemoryContext::kCount)];
};

}  // namespace memory
}  // namespace base

#endif  // BASE_MEMORY_COBALT_RESIDENT_MEMORY_OBSERVER_H_
