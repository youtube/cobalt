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

#include "base/memory/cobalt_resident_memory_observer.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace base {
namespace memory {

// static
CobaltResidentMemoryObserver* CobaltResidentMemoryObserver::Get() {
  static base::NoDestructor<CobaltResidentMemoryObserver> instance;
  return instance.get();
}

CobaltResidentMemoryObserver::CobaltResidentMemoryObserver() {
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    UNSAFE_BUFFERS(counters_[i].value.store(0, std::memory_order_relaxed));
  }
}

void CobaltResidentMemoryObserver::Start() {
  base::PoissonAllocationSampler::Get()->AddSamplesObserver(this);
}

void CobaltResidentMemoryObserver::Stop() {
  base::PoissonAllocationSampler::Get()->RemoveSamplesObserver(this);
}

void CobaltResidentMemoryObserver::SampleAdded(
    void* address,
    size_t size,
    size_t total,
    base::allocator::dispatcher::AllocationSubsystem type,
    const char* context) {
  // Enforce reentrancy protection. The PoissonAllocationSampler guarantees
  // that ScopedMuteThreadSamples is active before calling observers.
  DCHECK(base::PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());

  MemoryContext memory_context = GetCurrentMemoryContext();
  uint8_t ctx = static_cast<uint8_t>(memory_context);

  if (ctx >= static_cast<uint8_t>(MemoryContext::kCount)) {
    return;
  }

  {
    base::AutoLock lock(mutex_);
    live_samples_.insert_or_assign(address, SampleData{ctx, total});
  }

  // Update the counter outside the lock to minimize contention.
  UNSAFE_BUFFERS(counters_[ctx].value.fetch_add(total, std::memory_order_relaxed));
}

void CobaltResidentMemoryObserver::SampleRemoved(void* address) {
  uint8_t ctx;
  size_t total;
  {
    base::AutoLock lock(mutex_);
    auto it = live_samples_.find(address);
    if (it == live_samples_.end()) {
      return;
    }
    ctx = it->second.memory_context;
    total = it->second.total;
    live_samples_.erase(it);
  }

  if (ctx < static_cast<uint8_t>(MemoryContext::kCount)) {
    UNSAFE_BUFFERS(counters_[ctx].value.fetch_sub(total, std::memory_order_relaxed));
  }
}

}  // namespace memory
}  // namespace base
